#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "math_utils.h"
#include "ofdm.h"

static void zero_result(ofdm_result_t *result) {
  memset(result, 0, sizeof(*result));
}

void ofdm_result_free(ofdm_result_t *result) {
  if (result == NULL) {
    return;
  }

  free(result->tx_freq);
  free(result->tx_time);
  free(result->tx_cp);
  free(result->channel_impulse);
  free(result->rx_after_channel);
  free(result->rx_after_noise);
  free(result->rx_no_cp);
  free(result->rx_freq);
  free(result->channel_freq);
  free(result->equalized_freq);
  free(result->ml_decision);
  zero_result(result);
}

int ofdm_run(const ofdm_cfg_t *config, ofdm_result_t *result) {
  // Default channel impulse response h[n] with 5 taps
  // Represents multipath propagation: line-of-sight (1.0) + delayed echoes (0.5, 0.3)
  static const double complex default_h[] = {
    1.0 + 0.0 * I,     // h[0]: primary path (LOS)
    0.0 + 0.0 * I,     // h[1]: no signal at this delay
    0.5 + 0.0 * I,     // h[2]: first delayed reflection (0.5 amplitude)
    0.0 + 0.0 * I,     // h[3]: no signal at this delay
    0.3 + 0.0 * I,     // h[4]: second delayed reflection (0.3 amplitude)
  };

  if (config == NULL || result == NULL) {
    return 1;
  }

  zero_result(result);

  const int n = config->n;           // Number of OFDM subcarriers
  const int cp = config->cp;         // Cyclic Prefix length
  const int ch_len = config->ch_len; // Channel impulse response length
  const double eq_eps = config->eq_eps;  // Regularization parameter for channel equalization
  const double snr_db = config->snr_db;  // Signal-to-Noise Ratio in dB

  double complex *tx_freq = NULL;    // Frequency domain transmitted symbols (complex)
  double complex *tx_time = NULL;    // Time domain signal after IFFT
  double complex *tx_cp = NULL;      // Time domain signal with CP prepended
  double complex *h = NULL;          // Channel impulse response
  double complex *rx_ch = NULL;      // Received signal after channel convolution
  double complex *rx_noisy = NULL;   // Received signal after AWGN noise addition
  double complex *rx_no_cp = NULL;   // Received signal with CP removed
  double complex *rx_freq = NULL;    // Received signal in frequency domain (after FFT)
  double complex *h_pad = NULL;      // Zero-padded channel for DFT
  double complex *h_freq = NULL;     // Channel frequency response (after FFT)
  double complex *eq_freq = NULL;    // Equalized frequency domain signal
  double complex *decided = NULL;    // Hard-decided symbols (QPSK: ±1/√2)

  // Allocate memory for TX processing
  tx_freq = malloc((size_t)n * sizeof(*tx_freq));
  tx_time = malloc((size_t)n * sizeof(*tx_time));
  tx_cp = malloc((size_t)(n + cp) * sizeof(*tx_cp));
  h = calloc((size_t)ch_len, sizeof(*h));
  if (tx_freq == NULL || tx_time == NULL || tx_cp == NULL || h == NULL) {
    goto cleanup;
  }

  // Initialize channel impulse response from config or use default
  if (config->channel_taps != NULL && config->channel_taps_len > 0) {
    for (int i = 0; i < ch_len && i < config->channel_taps_len; i++) {
      h[i] = config->channel_taps[i];
    }
  } else {
    for (int i = 0; i < ch_len && i < (int)(sizeof(default_h) / sizeof(default_h[0])); i++) {
      h[i] = default_h[i];
    }
  }

  // ========== TRANSMITTER: Modulation and OFDM Symbol Generation ==========
  // 1. Generate random QPSK symbols with equal probability for I and Q components
  //    QPSK: each symbol is (±1/√2) + j(±1/√2), i.e., one of 4 constellation points
  //    Normalization by 1/√2 ensures unit average symbol power: E[|s|²] = 1
  srand(config->seed);
  for (int k = 0; k < n; k++) {
    // Randomly set in-phase (I) and quadrature (Q) components to ±1/√2
    int b_i = (rand() % 2) ? 1 : -1;  // In-phase bit (binary)
    int b_q = (rand() % 2) ? 1 : -1;  // Quadrature bit (binary)
    tx_freq[k] = (b_i + I * b_q) / sqrt(2.0);  // QPSK constellation point
  }

  // 2. IFFT: Convert N frequency-domain subcarrier symbols to N time-domain samples
  //    This is the inverse Discrete Fourier Transform (IDFT)
  //    Mathematical formula:
  //      x[n] = (1/N) * Σ_{k=0}^{N-1} X[k] * e^(j*2π*k*n/N)  for n = 0, 1, ..., N-1
  //    
  //    In OFDM, this implements parallel modulation:
  //    - Each frequency-domain symbol X[k] modulates a subcarrier at frequency f_k = k * Δf
  //    - The IFFT creates an orthogonal multicarrier waveform with no spectral overlap
  //    - The time-domain signal has reduced peak-to-average power ratio (PAPR) compared to single carrier
  dft(tx_freq, tx_time, n, 1);  // 1 indicates inverse transform (IFFT)

  // 3. Cyclic Prefix: Prepend the last CP samples of the OFDM symbol to the front
  //    Mathematical operation:
  //      TX_CP = [x[N-CP], x[N-CP+1], ..., x[N-1], x[0], x[1], ..., x[N-1]]
  //    
  //    Function in multipath channels:
  //    - Eliminates Inter-Symbol Interference (ISI) by padding with known samples
  //    - If channel length L ≤ CP, the received OFDM symbol with CP forms a circular convolution
  //    - Circular convolution → diagonal matrix in frequency domain → independent subchannel equalization
  //    
  //    Without CP: linear convolution creates ISI affecting up to 2 OFDM symbols
  //    With CP of length ≥ L: only the primary symbol is affected, no ISI to neighboring symbols
  for (int i = 0; i < cp; i++) {
    // Copy last CP samples from tx_time
    tx_cp[i] = tx_time[n - cp + i];
  }
  // Copy original OFDM symbol after CP
  for (int i = 0; i < n; i++) {
    tx_cp[cp + i] = tx_time[i];
  }

  // ========== CHANNEL: Multipath Fading and Convolution ==========
  // Allocate memory for RX processing
  const int rx_len = (n + cp) + ch_len - 1;  // Length after linear convolution
  rx_ch = calloc((size_t)rx_len, sizeof(*rx_ch));
  rx_noisy = calloc((size_t)rx_len, sizeof(*rx_noisy));
  rx_no_cp = calloc((size_t)n, sizeof(*rx_no_cp));
  rx_freq = calloc((size_t)n, sizeof(*rx_freq));
  h_pad = calloc((size_t)n, sizeof(*h_pad));
  h_freq = calloc((size_t)n, sizeof(*h_freq));
  eq_freq = calloc((size_t)n, sizeof(*eq_freq));
  decided = calloc((size_t)n, sizeof(*decided));
  if (
    rx_ch == NULL || rx_noisy == NULL || rx_no_cp == NULL || rx_freq == NULL ||
    h_pad == NULL || h_freq == NULL || eq_freq == NULL || decided == NULL
  ) {
    goto cleanup;
  }

  // 4. Channel Convolution: Apply multipath fading
  //    Mathematical formula (linear time-invariant channel):
  //      y[n] = Σ_{m=0}^{L-1} h[m] * x[n-m]
  //    
  //    Physical interpretation in multipath environment:
  //    - h[0]: direct path (line-of-sight) with unit gain
  //    - h[1], h[2], ...: delayed replicas from reflections/scattering
  //    - Each h[m] has amplitude and phase determined by propagation path
  //    
  //    Effect on OFDM:
  //    - With CP: y[n] = circular conv of h[m] and x[n] over the symbol period
  //    - In frequency: Y[k] = H[k] * X[k] (multiplication, not convolution!)
  //    - This is why CP is essential: it converts linear convolution to circular convolution
  convolve(tx_cp, n + cp, h, ch_len, rx_ch);

  // ========== RECEIVER: Noise and SNR Calculation ==========
  // 5. Calculate signal power from channel output
  //    Average power of received signal:
  //      P_signal = (1/M) * Σ_{n=0}^{M-1} |y[n]|²
  //    where M = length of received signal (n + cp + ch_len - 1)
  double signal_power = 0.0;
  for (int i = 0; i < rx_len; i++) {
    signal_power += pow(cabs(rx_ch[i]), 2.0);
  }
  signal_power /= (double)rx_len;

  // 6. Compute noise variance from SNR
  //    SNR relationship:
  //      SNR_dB = 10 * log10(SNR_linear)
  //      SNR_linear = 10^(SNR_dB / 10)
  //    
  //    Noise variance calculation:
  //      σ²_noise = P_signal / SNR_linear
  //    
  //    For AWGN with two independent components (I and Q):
  //      σ = √(σ²_noise / 2)  ← variance per I/Q channel
  //    
  //    This ensures the total noise power E[|n_I|² + |n_Q|²] = σ²_noise
  double noise_var = 0.0;
  if (snr_db >= 0.0) {
    double snr_linear = pow(10.0, snr_db / 10.0);
    noise_var = signal_power / snr_linear;
  }

  // 7. Add AWGN (Additive White Gaussian Noise)
  //    Received signal model:
  //      y_noisy[n] = y[n] + w[n]
  //    where w[n] ~ CN(0, σ²_noise) is complex Gaussian noise
  //    
  //    Decomposition into I and Q:
  //      w[n] = w_I[n] + j*w_Q[n]
  //      w_I[n], w_Q[n] ~ N(0, σ²_noise/2) independently
  //    
  //    Practical computation:
  //      σ = √(σ²_noise / 2)
  //      w_I[n] = σ * gaussian_rand()
  //      w_Q[n] = σ * gaussian_rand()
  for (int i = 0; i < rx_len; i++) {
    if (snr_db >= 0.0) {
      double sigma = sqrt(noise_var / 2.0);  // Noise std dev for I/Q channels
      double n_re = sigma * gaussian_rand(); // Real part of noise (I component)
      double n_im = sigma * gaussian_rand(); // Imaginary part of noise (Q component)
      rx_noisy[i] = rx_ch[i] + (n_re + I * n_im);
    } else {
      // No noise (noiseless channel)
      rx_noisy[i] = rx_ch[i];
    }
  }

  // ========== RECEIVER: CP Removal and FFT ==========
  // 8. Remove Cyclic Prefix
  //    Extract the N samples that correspond to the original OFDM symbol
  //    Extract only y_noisy[CP:CP+N] to get the pure OFDM symbol period
  //    
  //    After removing CP, the N samples contain:
  //    - Direct channel effect (circular convolution with h[m])
  //    - AWGN noise
  //    - Ready for frequency-domain processing (FFT)
  for (int i = 0; i < n; i++) {
    rx_no_cp[i] = rx_noisy[cp + i];
  }

  // 9. FFT: Convert N time-domain samples to N frequency-domain subcarriers
  //    Mathematical formula (Discrete Fourier Transform):
  //      Y[k] = Σ_{n=0}^{N-1} y[n] * e^(-j*2π*k*n/N)  for k = 0, 1, ..., N-1
  //    
  //    OFDM interpretation:
  //    - Y[k] = received symbol at k-th subcarrier
  //    - Frequency separation: Δf = fs / N (fs = sampling rate)
  //    - Subcarrier spacing: f_k = k * Δf
  //    
  //    Channel effect in frequency domain (due to CP):
  //      Y[k] = H[k] * X[k] + W[k]
  //    where H[k] = Σ_{m=0}^{L-1} h[m] * e^(-j*2π*k*m/N) (channel frequency response)
  //          W[k] = FFT of noise (still white Gaussian)
  //    
  //    Key property: Each subcarrier experiences independent flat fading at its frequency
  dft(rx_no_cp, rx_freq, n, 0);  // 0 indicates forward transform (FFT)

  // ========== RECEIVER: Channel Estimation and Equalization ==========
  // 10. Compute Channel Frequency Response
  //     Zero-pad the channel impulse response to length N for FFT
  //     This converts the time-domain channel h[n] to frequency domain H[k]
  //     
  //     Padding with zeros:
  //       H_padded[n] = { h[n]          for n < L
  //                     { 0              for L ≤ n < N
  //     
  //     Why zero-padding?
  //     - Ensures FFT length matches the OFDM symbol FFT length (N samples)
  //     - Computes H[k] at the exact frequencies of OFDM subcarriers
  //     - Implicit periodicity assumption in FFT: assumes channel is periodic with period N
  for (int i = 0; i < n; i++) {
    h_pad[i] = (i < ch_len) ? h[i] : 0.0 + 0.0 * I;
  }

  // 11. FFT of zero-padded channel
  //     Channel frequency response:
  //       H[k] = Σ_{m=0}^{L-1} h[m] * e^(-j*2π*k*m/N)  for k = 0, 1, ..., N-1
  //     
  //     Interpretation:
  //     - H[k] represents the channel's frequency response at subcarrier k
  //     - Magnitude |H[k]|: attenuation at that frequency (fading magnitude)
  //     - Phase ∠H[k]: phase shift at that frequency
  //     
  //     Properties:
  //     - If channel is flat: all |H[k]| ≈ constant (same attenuation at all frequencies)
  //     - If channel is frequency-selective: |H[k]| varies with k (different gains per frequency)
  dft(h_pad, h_freq, n, 0);

  // ========== RECEIVER: Equalization and Decision ==========
  // 12. Zero-Forcing Channel Equalization (ZF) with Regularization
  //     Received frequency-domain equation:
  //       Y[k] = H[k] * X[k] + W[k]
  //     
  //     Ideal ZF equalizer (no regularization):
  //       X_eq[k] = Y[k] / H[k]
  //     Problem: Division by very small |H[k]| amplifies noise significantly
  //     
  //     Regularized ZF (Tikhonov regularization):
  //       X_eq[k] = Y[k] * conj(H[k]) / (|H[k]|² + ε)
  //     
  //     Derivation (using Hermitian transpose):
  //       = Y[k] * conj(H[k]) / (H[k] * conj(H[k]) + ε)
  //       = Matched filter output normalized by channel power + regularization
  //     
  //     Function of regularization parameter ε:
  //     - ε = 0: pure ZF (high noise amplification when |H[k]| is small)
  //     - ε > 0: trades off ZF performance vs. noise immunity
  //     - Typical value: ε = 1e-12 (very small, nearly pure ZF)
  //     
  //     In weak channel regions (|H[k]|² < ε):
  //       X_eq[k] ≈ Y[k] * conj(H[k]) / ε  (noise is amplified by ε)
  int symbol_errors = 0;
  int bit_errors = 0;
  for (int k = 0; k < n; k++) {
    // Magnitude squared of channel response at subcarrier k
    double h2 = pow(cabs(h_freq[k]), 2.0);
    
    // Zero-forcing equalizer with regularization
    // Matched filter: multiply by conj(H[k]) (phase alignment)
    // Normalization: divide by |H[k]|² (amplitude correction)
    // Regularization: add ε to denominator (prevent noise explosion)
    eq_freq[k] = (rx_freq[k] * conj(h_freq[k])) / (h2 + eq_eps);

    // 13. Hard Decision: QPSK Demodulation
    //     QPSK constellation: S = {(±1/√2) + j(±1/√2)}
    //     4 possible symbols at corners of unit square in complex plane
    //     
    //     Decision rule (minimum distance):
    //       Decide symbol closest to equalized received value X_eq[k]
    //     For QPSK (orthogonal I/Q components):
    //       d_I = +1/√2  if Re(X_eq[k]) ≥ 0,  else -1/√2
    //       d_Q = +1/√2  if Im(X_eq[k]) ≥ 0,  else -1/√2
    //       Decided[k] = d_I + j*d_Q
    //     
    //     Optimal only under Gaussian noise assumption
    //     Threshold at 0 is optimal when constellation points are equally likely
    double d_re = (creal(eq_freq[k]) >= 0.0) ? (1.0 / sqrt(2.0)) : (-1.0 / sqrt(2.0));
    double d_im = (cimag(eq_freq[k]) >= 0.0) ? (1.0 / sqrt(2.0)) : (-1.0 / sqrt(2.0));
    decided[k] = d_re + I * d_im;

    // 14. Error Counting
    //     Symbol Error: occurs when decided symbol ≠ transmitted symbol
    //       decision_error[k] = |decided[k] - tx_freq[k]| > threshold
    //     
    //     Threshold = 1e-9 accounts for floating-point precision
    //     For QPSK, minimum distance between adjacent symbols = √2/2 ≈ 0.707
    //     So threshold of 1e-9 < 0.707 means only exact matches are considered correct
    if (cabs(decided[k] - tx_freq[k]) > 1e-9) {
      symbol_errors++;
    }

    // Bit Error counting:
    // QPSK mapping: each symbol carries 2 bits (I-bit and Q-bit)
    //   bit_I = 0 if Re(s) = +1/√2,  bit_I = 1 if Re(s) = -1/√2
    //   bit_Q = 0 if Im(s) = +1/√2,  bit_Q = 1 if Im(s) = -1/√2
    // 
    // Comparison using sign (≥ 0.0):
    //   Re(decided[k]) ≥ 0  ↔ symbol I-component is +1/√2
    //   Re(tx_freq[k]) ≥ 0  ↔ transmitted I-component is +1/√2
    //   Error if signs differ → I-bit error
    if ((creal(decided[k]) >= 0.0) != (creal(tx_freq[k]) >= 0.0)) {
      bit_errors++;  // Error in I-channel bit
    }
    if ((cimag(decided[k]) >= 0.0) != (cimag(tx_freq[k]) >= 0.0)) {
      bit_errors++;  // Error in Q-channel bit
    }
  }

  // ========== Store Results ==========
  // Assign computed signals to result structure for output
  result->tx_freq = tx_freq;
  result->tx_freq_len = n;
  result->tx_time = tx_time;
  result->tx_time_len = n;
  result->tx_cp = tx_cp;
  result->tx_cp_len = n + cp;
  result->channel_impulse = h;
  result->channel_impulse_len = ch_len;
  result->rx_after_channel = rx_ch;
  result->rx_after_channel_len = rx_len;
  result->rx_after_noise = rx_noisy;
  result->rx_after_noise_len = rx_len;
  result->rx_no_cp = rx_no_cp;
  result->rx_no_cp_len = n;
  result->rx_freq = rx_freq;
  result->rx_freq_len = n;
  result->channel_freq = h_freq;
  result->channel_freq_len = n;
  result->equalized_freq = eq_freq;
  result->equalized_freq_len = n;
  result->ml_decision = decided;
  result->ml_decision_len = n;
  result->signal_power = signal_power;
  result->noise_variance = noise_var;
  result->symbol_errors = symbol_errors;
  result->bit_errors = bit_errors;

  return 0;

cleanup:
  free(tx_freq);
  free(tx_time);
  free(tx_cp);
  free(h);
  free(rx_ch);
  free(rx_noisy);
  free(rx_no_cp);
  free(rx_freq);
  free(h_pad);
  free(h_freq);
  free(eq_freq);
  free(decided);
  zero_result(result);
  return 1;
}
