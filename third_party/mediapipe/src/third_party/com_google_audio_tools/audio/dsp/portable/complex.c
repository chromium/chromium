#include "audio/dsp/portable/complex.h"

/* Copies the sign of y to x. Used below in several inverse functions. */
static double CopySign(double x, double y) {
  x = fabs(x);
  return (y < 0.0) ? -x : x;
}

ComplexDouble ComplexDoubleDiv(ComplexDouble w, ComplexDouble z) {
  int exponent;
  /* To avoid over or underflow when calculating the denominator, scale the
   * component with larger magnitude into the range [0.5, 1).
   */
  if (fabs(z.real) >= fabs(z.imag)) {
    z.real = frexp(z.real, &exponent);
    z.imag = ldexp(z.imag, -exponent);  /* Compute z.imag *= 2^-exponent. */
  } else {
    z.imag = frexp(z.imag, &exponent);
    z.real = ldexp(z.real, -exponent);  /* Compute z.real *= 2^-exponent. */
  }

  const double denom = z.real * z.real + z.imag * z.imag;
  ComplexDouble result;
  result.real = ldexp((w.real * z.real + w.imag * z.imag) / denom, -exponent);
  result.imag = ldexp((w.imag * z.real - w.real * z.imag) / denom, -exponent);
  return result;
}

double ComplexDoubleAbs(ComplexDouble z) {
  z.real = fabs(z.real);
  z.imag = fabs(z.imag);

  /* Get a = max(z.real, z.imag), b = min(z.real, z.imag). */
  double a = (z.real < z.imag) ? z.imag : z.real;
  double b = (z.real < z.imag) ? z.real : z.imag;
  int exponent;
  /* To avoid `a * a` from over or underflowing in the following step, scale `a`
   * to the range [0.5, 1) and scale `b` by the same factor. In the edge case
   * that `a` is zero, frexp returns zero and sets exponent = 0.
   */
  a = frexp(a, &exponent);
  b = ldexp(b, -exponent);  /* Compute b *= 2^-exponent. */

  return ldexp(sqrt(a * a + b * b), exponent);
}

ComplexDouble ComplexDoubleSqrt(ComplexDouble z) {
  /* For z = x + iy, if y != 0, its principal branch square root is
   *
   *   sqrt(z) = sqrt((|z| + x) / 2) + i sign(y) sqrt((|z| - x) / 2).
   *
   * [This can be derived by expressing sqrt(z) in polar coordinates and
   * applying half-angle trig identities.] The above expression is numerically
   * unstable for |y| << |x|, since (|z| - |x|) suffers loss of significance. To
   * avoid this, we approach its computation in two cases.
   */
  if (z.real == 0.0) {
    /* For purely imaginary input, z = iy, the two sqrts are equal and
     *
     *   sqrt(z) = sqrt(|z| / 2) (1 + i sign(y)).
     */
    const double q = sqrt(fabs(z.imag) / 2);
    return ComplexDoubleMake(q, z.imag < 0.0 ? -q : q);
  } else {
    /* Otherwise, we compute q = sqrt((|z| + |x|) / 2) and get the other term as
     *
     *   sqrt((|z| - |x|) / 2)
     *
     *       sqrt((|z| - |x|) / 2) q   sqrt(|z|^2 - x^2) / 2   |y|
     *     = ----------------------- = --------------------- = ---.
     *                  q                        q             2 q
     *
     * Since x != 0, we are sure to have q > 0, so there is no division by zero.
     * This computation also does the right thing for purely real input (y = 0),
     * in which case q = sqrt(|x|).
     */
    const double q = sqrt((ComplexDoubleAbs(z) + fabs(z.real)) / 2);
    if (z.real > 0.0) {
      /* If z is purely real and > 0, then output imag part is exactly zero. */
      return ComplexDoubleMake(q, z.imag / (2 * q));
    } else {
      /* If z is purely real and < 0, then output real part is exactly zero. */
      return ComplexDoubleMake(fabs(z.imag) / (2 * q), z.imag < 0.0 ? -q : q);
    }
  }
}

ComplexDouble ComplexDoubleExp(ComplexDouble z) {
  const double r = exp(z.real);
  ComplexDouble result;
  result.real = r * cos(z.imag);
  result.imag = r * sin(z.imag);
  return result;
}

ComplexDouble ComplexDoubleLog(ComplexDouble z) {
  ComplexDouble result;
  result.real = log(ComplexDoubleAbs(z));
  result.imag = ComplexDoubleArg(z);
  return result;
}

/* In the complex plane, trig and hyperbolic trig functions are related by
 *   cosh(x + i y) = cosh(x) cos(y) + i sinh(x) sin(y),
 *   sinh(x + i y) = sinh(x) cos(y) + i cosh(x) sin(y),
 *   cos(x + i y) = cos(x) cosh(y) - i sin(x) sinh(y),
 *   sin(x + i y) = sin(x) cosh(y) + i cos(x) sinh(y).
 */

ComplexDouble ComplexDoubleCosh(ComplexDouble z) {
  ComplexDouble result;
  result.real = cosh(z.real) * cos(z.imag);
  result.imag = sinh(z.real) * sin(z.imag);
  return result;
}

ComplexDouble ComplexDoubleSinh(ComplexDouble z) {
  ComplexDouble result;
  result.real = sinh(z.real) * cos(z.imag);
  result.imag = cosh(z.real) * sin(z.imag);
  return result;
}

ComplexDouble ComplexDoubleCos(ComplexDouble z) {
  ComplexDouble result;
  result.real = cos(z.real) * cosh(z.imag);
  result.imag = -sin(z.real) * sinh(z.imag);
  return result;
}

ComplexDouble ComplexDoubleSin(ComplexDouble z) {
  ComplexDouble result;
  result.real = sin(z.real) * cosh(z.imag);
  result.imag = cos(z.real) * sinh(z.imag);
  return result;
}

ComplexDouble ComplexDoubleACosh(ComplexDouble z) {
  /* Compute log(z + sqrt(z^2 - 1)). */
  ComplexDouble result = ComplexDoubleLog(ComplexDoubleAdd(
      z, ComplexDoubleSqrt(ComplexDoubleMake(
             z.real * z.real - z.imag * z.imag - 1.0, 2 * z.real * z.imag))));
  result.real = fabs(result.real);
  result.imag = CopySign(result.imag, z.imag);
  return result;
}

ComplexDouble ComplexDoubleASinh(ComplexDouble z) {
  /* Compute log(z + sqrt(z^2 + 1)). */
  ComplexDouble result = ComplexDoubleLog(ComplexDoubleAdd(
      z, ComplexDoubleSqrt(ComplexDoubleMake(
             z.real * z.real - z.imag * z.imag + 1.0, 2 * z.real * z.imag))));
  result.real = CopySign(result.real, z.real);
  result.imag = CopySign(result.imag, z.imag);
  return result;
}

ComplexDouble ComplexDoubleACos(ComplexDouble z) {
  /* Compute log(z + sqrt(z^2 - 1)). */
  ComplexDouble result = ComplexDoubleLog(ComplexDoubleAdd(
      z, ComplexDoubleSqrt(ComplexDoubleMake(
             z.real * z.real - z.imag * z.imag - 1.0, 2 * z.real * z.imag))));
  return ComplexDoubleMake(fabs(result.imag), CopySign(result.real, -z.imag));
}

ComplexDouble ComplexDoubleASin(ComplexDouble z) {
  /* Compute asin(z) = -i asinh(i z). */
  ComplexDouble w = ComplexDoubleASinh(ComplexDoubleMake(-z.imag, z.real));
  return ComplexDoubleMake(w.imag, -w.real);
}
