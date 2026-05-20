#ifndef OFDM_H
#define OFDM_H

#include <complex.h>

typedef struct {
  int n;
  int cp;
  int ch_len;
  double eq_eps;
  double snr_db;
  unsigned int seed;
  const double complex *channel_taps;
  int channel_taps_len;
} ofdm_cfg_t;

typedef struct {
  double complex *tx_freq;
  int tx_freq_len;
  double complex *tx_time;
  int tx_time_len;
  double complex *tx_cp;
  int tx_cp_len;
  double complex *channel_impulse;
  int channel_impulse_len;
  double complex *rx_after_channel;
  int rx_after_channel_len;
  double complex *rx_after_noise;
  int rx_after_noise_len;
  double complex *rx_no_cp;
  int rx_no_cp_len;
  double complex *rx_freq;
  int rx_freq_len;
  double complex *channel_freq;
  int channel_freq_len;
  double complex *equalized_freq;
  int equalized_freq_len;
  double complex *ml_decision;
  int ml_decision_len;
  double signal_power;
  double noise_variance;
  int symbol_errors;
  int bit_errors;
} ofdm_result_t;

int ofdm_run(const ofdm_cfg_t *config, ofdm_result_t *result);
void ofdm_result_free(ofdm_result_t *result);

#endif  // OFDM_H
