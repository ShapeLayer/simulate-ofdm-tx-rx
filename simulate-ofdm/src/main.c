#include <complex.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "math_utils.h"
#include "ofdm.h"

#define DEFAULT_N 64
#define DEFAULT_CP 16
#define DEFAULT_CH_LEN 5
#define DEFAULT_EQ_EPS 1e-12

#pragma region Utility functions for main
static void save_complex_csv(
  const char *path,
  const double complex *x,
  int n
) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open %s\n", path);
    exit(1);
  }
  fprintf(fp, "index,real,imag\n");
  for (int i = 0; i < n; i++) {
    fprintf(fp, "%d,%.12f,%.12f\n", i, creal(x[i]), cimag(x[i]));
  }
  fclose(fp);
}

static void print_usage(void) {
  printf("Usage: ./simulate-ofdm [--n=64 --cp=16 --ch-len=5 --channel-taps=1,0,0.5,0,0.3 --eq-eps=1e-12 --snr-db=20.0 --seed=7 --output-dir=output|help]\n");
}

static int ensure_directory_recursive(const char *path) {
  char *buffer = malloc(strlen(path) + 1);
  if (buffer == NULL) {
    return 0;
  }
  strcpy(buffer, path);

  for (char *cursor = buffer + 1; *cursor != '\0'; cursor++) {
    if (*cursor == '/') {
      *cursor = '\0';
      if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        free(buffer);
        return 0;
      }
      *cursor = '/';
    }
  }

  if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
    free(buffer);
    return 0;
  }

  free(buffer);
  return 1;
}

static char *join_path(const char *dir, const char *name) {
  size_t dir_len = strlen(dir);
  size_t name_len = strlen(name);
  int needs_sep = (dir_len > 0 && dir[dir_len - 1] != '/');
  char *path = malloc(dir_len + (size_t)needs_sep + name_len + 1);
  if (path == NULL) {
    return NULL;
  }
  memcpy(path, dir, dir_len);
  if (needs_sep) {
    path[dir_len] = '/';
    memcpy(path + dir_len + 1, name, name_len);
    path[dir_len + 1 + name_len] = '\0';
  } else {
    memcpy(path + dir_len, name, name_len);
    path[dir_len + name_len] = '\0';
  }
  return path;
}
#pragma endregion

#pragma region Parse comma-separated real taps into complex array
/**
  * Parses a comma-separated list of real numbers into an array of complex numbers with zero imaginary parts. Returns 1 on success, 0 on failure. The caller is responsible for freeing the allocated array.
  * @param spec The input string containing comma-separated real numbers.
  * @param taps_out Output pointer to the allocated array of complex numbers.
  * @param tap_count_out Output pointer to the number of taps parsed.
  * @return 1 on success, 0 on failure.
  */
static int parse_real_taps(
  const char *spec,
  double complex **taps_out,
  int *tap_count_out
) {
  char *buffer = malloc(strlen(spec) + 1);
  if (buffer == NULL) {
    return 0;
  }
  strcpy(buffer, spec);

  int count = 1;
  for (const char *p = spec; *p != '\0'; p++) {
    if (*p == ',') {
      count++;
    }
  }

  double complex *taps = calloc((size_t)count, sizeof(*taps));
  if (taps == NULL) {
    free(buffer);
    return 0;
  }

  int index = 0;
  char *cursor = buffer;
  while (*cursor != '\0') {
    char *end = NULL;
    double value = strtod(cursor, &end);
    if (end == cursor) {
      free(buffer);
      free(taps);
      return 0;
    }

    taps[index++] = value + 0.0 * I;
    cursor = end;
    while (*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if (*cursor == ',') {
      cursor++;
      while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
      }
      continue;
    }
    if (*cursor != '\0') {
      free(buffer);
      free(taps);
      return 0;
    }
  }

  free(buffer);
  *taps_out = taps;
  *tap_count_out = index;
  return 1;
}
#pragma endregion

#pragma region Parse runtime arguments
int main(int argc, char *argv[]) {
  int n = DEFAULT_N;
  int cp = DEFAULT_CP;
  int ch_len = DEFAULT_CH_LEN;
  double eq_eps = DEFAULT_EQ_EPS;
  double snr_db = -1.0;
  unsigned int seed = 7;
  char *output_dir = NULL;
  char *h_spec = NULL;
  double complex *parsed_h = NULL;
  int parsed_h_len = 0;

  const char *snr_env = getenv("SNR_DB");
  if (snr_env != NULL) {
    snr_db = atof(snr_env);
  }

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (strcmp(arg, "help") == 0 || strcmp(arg, "--help") == 0) {
      print_usage();
      return 0;
    }

    const char *flag = arg;
    if (strncmp(flag, "--", 2) == 0) {
      flag += 2;
    } else {
      fprintf(stderr, "Invalid argument: %s\n", arg);
      print_usage();
      return 1;
    }

    const char *eq_pos = strchr(flag, '=');
    if (eq_pos == NULL) {
      fprintf(stderr, "Invalid argument: %s\n", arg);
      print_usage();
      return 1;
    }

    char key[32];
    size_t key_len = (size_t)(eq_pos - flag);
    if (key_len == 0 || key_len >= sizeof(key)) {
      fprintf(stderr, "Invalid argument: %s\n", arg);
      return 1;
    }

    memcpy(key, flag, key_len);
    key[key_len] = '\0';

    const char *value = eq_pos + 1;
    if (strcmp(key, "n") == 0) {
      n = atoi(value);
    } else if (strcmp(key, "cp") == 0) {
      cp = atoi(value);
    } else if (strcmp(key, "ch-len") == 0) {
      ch_len = atoi(value);
    } else if (strcmp(key, "eq-eps") == 0) {
      eq_eps = atof(value);
    } else if (strcmp(key, "snr-db") == 0) {
      snr_db = atof(value);
    } else if (strcmp(key, "seed") == 0) {
      seed = (unsigned int)strtoul(value, NULL, 10);
    } else if (strcmp(key, "output-dir") == 0) {
      free(output_dir);
      output_dir = malloc(strlen(value) + 1);
      if (output_dir == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free(h_spec);
        return 1;
      }
      strcpy(output_dir, value);
    } else if (strcmp(key, "channel-taps") == 0) {
      free(h_spec);
      h_spec = malloc(strlen(value) + 1);
      if (h_spec == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
      }
      strcpy(h_spec, value);
    } else {
      fprintf(stderr, "Unknown parameter: %s\n", key);
      print_usage();
      return 1;
    }
  }

  if (n <= 0 || cp < 0 || ch_len <= 0 || eq_eps <= 0.0) {
    fprintf(stderr, "Invalid parameters\n");
    return 1;
  }
  if (cp > n) {
    fprintf(stderr, "CP must be less than or equal to N\n");
    return 1;
  }
  if (ch_len > n) {
    fprintf(stderr, "CH_LEN must be less than or equal to N\n");
    return 1;
  }

  if (output_dir == NULL) {
    output_dir = malloc(strlen("output") + 1);
    if (output_dir == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      free(h_spec);
      return 1;
    }
    strcpy(output_dir, "output");
  }

  if (!ensure_directory_recursive(output_dir)) {
    fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
    free(output_dir);
    free(h_spec);
    return 1;
  }

  if (h_spec != NULL) {
    if (!parse_real_taps(h_spec, &parsed_h, &parsed_h_len)) {
      fprintf(stderr, "Failed to parse channel-taps: %s\n", h_spec);
      free(output_dir);
      free(h_spec);
      return 1;
    }
    ch_len = parsed_h_len;
  }

  if (ch_len > n) {
    fprintf(stderr, "CH_LEN must be less than or equal to N\n");
    free(parsed_h);
    free(h_spec);
    return 1;
  }

  ofdm_cfg_t config = {
    .n = n,
    .cp = cp,
    .ch_len = ch_len,
    .eq_eps = eq_eps,
    .snr_db = snr_db,
    .seed = seed,
    .channel_taps = parsed_h,
    .channel_taps_len = parsed_h_len,
  };
  printf("Running OFDM simulation with N=%d, CP=%d, CH_LEN=%d, EQ_EPS=%.2e, SNR_DB=%.2f\n",
         config.n, config.cp, config.ch_len, config.eq_eps, config.snr_db);
#pragma endregion

#pragma region Simulate OFDM
  ofdm_result_t result;
  if (ofdm_run(&config, &result) != 0) {
    free(parsed_h);
    free(h_spec);
    free(output_dir);
    return 1;
  }
#pragma endregion
#pragma region Save results to csv files
  char *tx_freq_path = join_path(output_dir, "tx_freq_symbols.csv");
  char *tx_time_path = join_path(output_dir, "tx_time_ifft.csv");
  char *tx_cp_path = join_path(output_dir, "tx_with_cp.csv");
  char *channel_impulse_path = join_path(output_dir, "channel_impulse.csv");
  char *rx_after_channel_path = join_path(output_dir, "rx_after_channel.csv");
  char *rx_after_noise_path = join_path(output_dir, "rx_after_noise.csv");
  char *rx_no_cp_path = join_path(output_dir, "rx_no_cp.csv");
  char *rx_freq_path = join_path(output_dir, "rx_freq_fft.csv");
  char *channel_freq_path = join_path(output_dir, "channel_freq.csv");
  char *equalized_freq_path = join_path(output_dir, "equalized_freq.csv");
  char *ml_decision_path = join_path(output_dir, "ml_decision.csv");
  char *metrics_path = join_path(output_dir, "metrics.txt");
  if (
    tx_freq_path == NULL || tx_time_path == NULL || tx_cp_path == NULL ||
    channel_impulse_path == NULL || rx_after_channel_path == NULL ||
    rx_after_noise_path == NULL || rx_no_cp_path == NULL || rx_freq_path == NULL ||
    channel_freq_path == NULL || equalized_freq_path == NULL || ml_decision_path == NULL ||
    metrics_path == NULL
  ) {
    fprintf(stderr, "Memory allocation failed\n");
    free(tx_freq_path);
    free(tx_time_path);
    free(tx_cp_path);
    free(channel_impulse_path);
    free(rx_after_channel_path);
    free(rx_after_noise_path);
    free(rx_no_cp_path);
    free(rx_freq_path);
    free(channel_freq_path);
    free(equalized_freq_path);
    free(ml_decision_path);
    free(metrics_path);
    ofdm_result_free(&result);
    free(parsed_h);
    free(h_spec);
    free(output_dir);
    return 1;
  }

  save_complex_csv(tx_freq_path, result.tx_freq, result.tx_freq_len);
  save_complex_csv(tx_time_path, result.tx_time, result.tx_time_len);
  save_complex_csv(tx_cp_path, result.tx_cp, result.tx_cp_len);
  save_complex_csv(channel_impulse_path, result.channel_impulse, result.channel_impulse_len);
  save_complex_csv(rx_after_channel_path, result.rx_after_channel, result.rx_after_channel_len);
  save_complex_csv(rx_after_noise_path, result.rx_after_noise, result.rx_after_noise_len);
  save_complex_csv(rx_no_cp_path, result.rx_no_cp, result.rx_no_cp_len);
  save_complex_csv(rx_freq_path, result.rx_freq, result.rx_freq_len);
  save_complex_csv(channel_freq_path, result.channel_freq, result.channel_freq_len);
  save_complex_csv(equalized_freq_path, result.equalized_freq, result.equalized_freq_len);
  save_complex_csv(ml_decision_path, result.ml_decision, result.ml_decision_len);

  FILE *fp = fopen(metrics_path, "w");
  if (!fp) {
    fprintf(stderr, "Failed to write metrics file\n");
    ofdm_result_free(&result);
    free(tx_freq_path);
    free(tx_time_path);
    free(tx_cp_path);
    free(channel_impulse_path);
    free(rx_after_channel_path);
    free(rx_after_noise_path);
    free(rx_no_cp_path);
    free(rx_freq_path);
    free(channel_freq_path);
    free(equalized_freq_path);
    free(ml_decision_path);
    free(metrics_path);
    free(parsed_h);
    free(h_spec);
    free(output_dir);
    return 1;
  }
  fprintf(fp, "N=%d\n", result.tx_freq_len);
  fprintf(fp, "CP=%d\n", cp);
  fprintf(fp, "channel_length=%d\n", result.channel_impulse_len);
  fprintf(fp, "snr_db=%.2f\n", snr_db);
  fprintf(fp, "signal_power=%.12f\n", result.signal_power);
  fprintf(fp, "noise_variance=%.12f\n", result.noise_variance);
  fprintf(fp, "symbol_errors=%d\n", result.symbol_errors);
  fprintf(fp, "bit_errors=%d\n", result.bit_errors);
  fprintf(fp, "SER=%.12f\n", (double)result.symbol_errors / (double)result.tx_freq_len);
  fprintf(fp, "BER=%.12f\n", (double)result.bit_errors / (double)(2 * result.tx_freq_len));
  fclose(fp);

  printf("Simulation complete. CSV files are in %s\n", output_dir);
  printf("Symbol errors: %d / %d\n", result.symbol_errors, result.tx_freq_len);
  printf("Bit errors: %d / %d\n", result.bit_errors, 2 * result.tx_freq_len);

#pragma endregion
#pragma region Cleanup
  ofdm_result_free(&result);
  free(tx_freq_path);
  free(tx_time_path);
  free(tx_cp_path);
  free(channel_impulse_path);
  free(rx_after_channel_path);
  free(rx_after_noise_path);
  free(rx_no_cp_path);
  free(rx_freq_path);
  free(channel_freq_path);
  free(equalized_freq_path);
  free(ml_decision_path);
  free(metrics_path);
  free(parsed_h);
  free(h_spec);
  free(output_dir);
  return 0;
}
#pragma endregion
