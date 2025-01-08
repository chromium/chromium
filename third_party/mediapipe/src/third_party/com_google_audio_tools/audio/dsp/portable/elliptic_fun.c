#include "audio/dsp/portable/elliptic_fun.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include "audio/dsp/portable/math_constants.h"

/* Maximum Landen sequence length. The Landen recursion converges rapidly,
 * needing five or fewer iterations when parameter m [square of the elliptic
 * modulus] is less than 0.99.
 */
#define kMaxLandenSequenceLength 10

/* Generates a sequence of moduli by the Landen recursion
 *
 *   k[n] = (k[n-1] / (1 + k'[n-1]))^2
 *
 * for parameter value m = k^2. This sequence is useful for K(m) and other
 * elliptic functions. See equation (50) and discussion in Orfanidis' notes
 * https://pdfs.semanticscholar.org/1131/3b5276e5deb428afb7dc208c9120460fbc4a.pdf
 * The `moduli` arg should be an array of size kMaxLandenSequenceLength. The
 * return value is the number of moduli written.
 */
static int GenerateLandenSequence(double m, double* moduli) {
  double elliptic_modulus = sqrt(m);
  int num_moduli = 1;
  moduli[0] = elliptic_modulus;
  while (num_moduli < kMaxLandenSequenceLength &&
         elliptic_modulus > DBL_EPSILON) {
    /* Make a Landen transformation. */
    double temp = elliptic_modulus /
        (1.0 + sqrt(1.0 - elliptic_modulus * elliptic_modulus));
    elliptic_modulus = temp * temp;
    moduli[num_moduli++] = elliptic_modulus;
  }
  if (elliptic_modulus > DBL_EPSILON) {
    fprintf(stderr, "Warning: "
            "Landen sequence did not converge, results may be inaccurate.\n");
  }
  return num_moduli;
}

/* Compute K(m) based on Landen transformation
 * [https://en.wikipedia.org/wiki/Landen%27s_transformation], which expresses
 * elliptic integrals in terms of elliptic integrals with mapped parameters.
 *
 * NOTE: An alternative to compute K(m) is the arithmetic-geometric mean (AGM)
 * process [section 17.6 in http://people.math.sfu.ca/~cbm/aands/page_598.htm].
 */
static double EllipticKFromLanden(const double* moduli, int num_moduli) {
  double result = M_PI / 2;
  int i;
  for (i = 1; i < num_moduli; ++i) {
    result *= 1.0 + moduli[i];
  }
  return result;
}

double EllipticK(double m) {
  if (m == 1.0) {
    return HUGE_VAL;
  } else if (m > 1.0 - 1e-12) {
    /* Use asymptotic approximation near m = 1, where the function has a
     * singularity. [This formula is between equations (53) and (54) in
     * Orfanidis' notes.]
     */
    const double m1 = 1.0 - m;
    const double temp = -0.5 * log(m1 / 16.0);
    return temp + (temp - 1.0) * m1 / 4;
  } else {
    double moduli[kMaxLandenSequenceLength];
    int num_moduli = GenerateLandenSequence(m, moduli);
    return EllipticKFromLanden(moduli, num_moduli);
  }
}

ComplexDouble EllipticF(ComplexDouble phi, double m) {
  if (ComplexDoubleAbs2(phi) < 1e-6) {
    /* For small phi, a simple Taylor approximation is more accurate than the
     * inverse cd method used below. Use the first two series terms from
     * http://reference.wolfram.com/language/ref/EllipticF.html
     *
     * Compute: phi * (1 + phi^2 * (m / 6)).
     */
    return ComplexDoubleMul(
        phi, ComplexDoubleAdd(
                 ComplexDoubleMake(1.0, 0.0),
                 ComplexDoubleMulReal(ComplexDoubleSquare(phi), m / 6.0)));
  } else {
    /* To evaluate u = F(phi|m), compute
     *   w0 = sin(phi)
     * so that w0 = sn(u|m), then apply equation (56) for inverting sn(u|m)
     * described in Orfanidis' notes.
     */
    double moduli[kMaxLandenSequenceLength];
    int num_moduli = GenerateLandenSequence(m, moduli);
    ComplexDouble w = ComplexDoubleSin(phi);
    int i;
    for (i = 1; i < num_moduli; ++i) {
      /* Compute
       *   w /= (1 + sqrt(1 - (w * moduli[i - 1])^2)) * 0.5 * (1 + moduli[i]).
       */
      w = ComplexDoubleDiv(
          w, ComplexDoubleMulReal(
                 ComplexDoubleAdd(ComplexDoubleMake(1.0, 0.0),
                                  ComplexDoubleSqrt(ComplexDoubleSub(
                                      ComplexDoubleMake(1.0, 0.0),
                                      ComplexDoubleSquare(ComplexDoubleMulReal(
                                          w, moduli[i - 1]))))),
                 0.5 * (1.0 + moduli[i])));
    }

    /* Compute asin(w) * (2 / M_PI) * EllipticKFromLanden(). */
    return ComplexDoubleMulReal(
        ComplexDoubleASin(w),
        (2 / M_PI) * EllipticKFromLanden(moduli, num_moduli));
  }
}

ComplexDouble JacobiAmplitude(ComplexDouble u, double m) {
  /* To evaluate phi = am(u|m), use descending Landen transformation (Gauss'
   * transformation) to compute sn(u|m), then get phi = asin(sn(u|m)).
   *
   * The recurrence to compute sn(u|m) is
   *   w_{n-1} = w_n (1 + k_n) / (1 + k_n w_n^2),
   * where k_n is the nth Landen sequence modulus for parameter m. The
   * recurrence is initialized with w_N = sin((pi/2) u / K(m)) and ends with
   * w_0 = sn(u|m). This recurrence is equation (55) in Orfanidis' notes or
   * equation (16.12.2) in http://people.math.sfu.ca/~cbm/aands/page_573.htm.
   *
   * NOTE: An alternative is the AGM-based recurrence in equations (16.4.2-3)
   * from http://people.math.sfu.ca/~cbm/aands/page_571.htm.
   */
  double moduli[kMaxLandenSequenceLength];
  int num_moduli = GenerateLandenSequence(m, moduli);
  /* Compute w = sin(u * (M_PI / 2) / EllipticKFromLanden()). */
  ComplexDouble w = ComplexDoubleSin(ComplexDoubleMulReal(
      u, (M_PI / 2) / EllipticKFromLanden(moduli, num_moduli)));
  int i;
  for (i = num_moduli - 1; i > 0; --i) {
    const double k = moduli[i];
    /* Compute w *= (1 + k) / (1 + w^2 * k). */
    w = ComplexDoubleMulReal(
        ComplexDoubleDiv(w, ComplexDoubleAdd(ComplexDoubleMake(1.0, 0.0),
                                             ComplexDoubleMulReal(
                                                 ComplexDoubleSquare(w), k))),
        1.0 + k);
  }
  return ComplexDoubleASin(w);
}
