#include <complex.h>
#include <math.h>
#include <stdlib.h>

#include "math_utils.h"

void dft(
  const double complex *in,
  double complex *out,
  int n, int inverse
) {
  for (int k = 0; k < n; k++) {
    double complex acc = 0.0 + 0.0 * I;
    for (int t = 0; t < n; t++) {
      double angle = 2.0 * M_PI * (double)k * (double)t / (double)n;
      double complex w = cos(angle) + (inverse ? I : -I) * sin(angle);
      acc += in[t] * w;
    }
    out[k] = inverse ? (acc / (double)n) : acc;
  }
}

void convolve(
  const double complex *x, int nx,
  const double complex *h, int nh,
  double complex *y
) {
  int ny = nx + nh - 1;
  for (int n = 0; n < ny; n++) {
    double complex acc = 0.0 + 0.0 * I;
    for (int k = 0; k < nh; k++) {
      int xi = n - k;
      if (xi >= 0 && xi < nx) {
        acc += x[xi] * h[k];
      }
    }
    y[n] = acc;
  }
}

double gaussian_rand(void) {
  double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
  double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
  return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}
