#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void dft(
  const double complex *in,
  double complex *out,
  int n,
  int inverse
);

void convolve(
  const double complex *x, int nx,
  const double complex *h, int nh,
  double complex *y
);

double gaussian_rand(void);

#endif  // MATH_UTILS_H
