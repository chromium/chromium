#include "audio/dsp/portable/number_util.h"

#include <limits.h>
#include <math.h>

const RationalApproximationOptions kRationalApproximationDefaultOptions = {
  /*max_terms=*/47,
  /*precision=*/1e-9,
};

int RoundUpToMultiple(int value, int factor) {
  if (value < 1) {
    return value + ((-value) % factor);
  } else {
    /* To round a positive value up to a multiple of factor, we want
     *
     *    ceil(value / factor) * factor
     *    = ((value + factor - 1) / factor) * factor   [integer division]
     *    = ((value - 1) / factor) * factor + factor.
     *
     * Noting that the `((value - 1) / factor) * factor` term is (value - 1)
     * rounded down to a multiple of factor, we can simplify it as
     *
     *    = (value - 1) - ((value - 1) % factor) + factor
     *    = value + factor - 1 - ((value - 1) % factor).
     */
    return value + factor - 1 - ((value - 1) % factor);
  }
}

int GreatestCommonDivisor(int a, int b) {
  while (b != 0) {
    int remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

typedef struct {
  int a;
  int b;
} Fraction;

static double FractionToDouble(const Fraction* rational) {
  return ((double) rational->a) / rational->b;
}

/* Suppose that `convergent` and `prev_convergent` are respectively the N- and
 * (N - 1)-term convergents of a continued fraction representation. This
 * function computes the fraction obtained by appending a term as the (N + 1)th
 * continued fraction term. This update formula applies Theorem 1 from
 * https://en.wikipedia.org/wiki/Continued_fraction#Some_useful_theorems. The
 * resulting fraction is in reduced form by Corollary 1.
 */
static Fraction AppendContinuedFractionTerm(
    const Fraction* convergent, const Fraction* prev_convergent, int term) {
  Fraction result;
  result.a = term * convergent->a + prev_convergent->a;
  result.b = term * convergent->b + prev_convergent->b;
  return result;
}

void RationalApproximation(double value, int max_denominator,
                           const RationalApproximationOptions* options,
                           int* out_numerator,
                           int* out_denominator) {
  /* Algorithm: We apply these rules to get the best rational approximation.
   *
   * "Rule 1. Truncate the continued fraction, and reduce its last term by a
   *    chosen amount (possibly zero).
   *
   * Rule 2. The reduced term cannot have less than half its original value.
   *
   * Rule 3. If the final term is even, half its value is admissible only if the
   *    corresponding semiconvergent is better than the previous convergent."
   * [https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations]
   *
   * We perform continued fraction expansion, truncating once the result would
   * exceeds max_denominator or would overflow int range. Then we use rules 2
   * and 3 to possibly append a final continued fraction term.
   *
   * Besides limiting the denominator, it is necessary to guard against overflow
   * since quantities grow rapidly in the continued fraction expansion. When
   * appending a continued fraction term n, the result is
   *
   *   result.a = n * convergent.a + prev_convergent.a,
   *   result.b = n * convergent.b + prev_convergent.b.
   *
   * We require result.a <= INT_MAX and result.b <= max_denominator, which
   * implies that n may be no larger than
   *
   *   floor(min{(INT_MAX - prev_convergent.a) / convergent.a,
   *             (max_denominator - prev_convergent.b) / convergent.b,
   *             1 / residual}).
   *
   * Additionally rule 2 implies that n may be no less than
   *
   *    floor(1 / residual) / 2.
   */
  if (max_denominator <= 0) {
    *out_numerator = 0;
    *out_denominator = 0;
    return;
  } else if (value > INT_MAX - 0.5) {
    *out_numerator = INT_MAX;
    *out_denominator = 1;
    return;
  } else if (value < INT_MIN + 0.5) {
    *out_numerator = INT_MIN;
    *out_denominator = 1;
    return;
  }

  const int sign = (value < 0.0) ? -1 : 1;
  /* From here on values are nonnegative. This simplifies overflowing checking
   * to needing only to bound from above. We reapply the sign to the result.
   */
  value = fabs(value);

  if (!(-value >= INT_MIN)) {  /* NAN value. */
    *out_numerator = 0;
    *out_denominator = 0;
    return;
  }

  if (!options) { options = &kRationalApproximationDefaultOptions; }

  double reciprocal_residual = value;
  double continued_fraction_term = floor(value);
  Fraction prev_convergent = {1, 0};
  Fraction convergent;
  convergent.a = (int)continued_fraction_term; /* Expand first term. */
  convergent.b = 1;

  int n = 0;
  /* [Rule 1] Build up the continued fraction representation, truncating once
   * the next convergent exceeds max_denominator. Two terms are added by the
   * logic before and after the loop, so start the loop at term=2.
   */
  int term;
  for (term = 2;; ++term) {
    const double next_residual = reciprocal_residual - continued_fraction_term;
    if (fabs(next_residual) <= options->convergence_tolerance) {
      *out_numerator = sign * convergent.a;
      *out_denominator = convergent.b;
      return; /* Continued fraction has converged. */
    }

    reciprocal_residual = 1.0 / next_residual;
    continued_fraction_term = floor(reciprocal_residual);

    /* Get next term upper bound so that denominator <= max_denominator. */
    n = (max_denominator - prev_convergent.b) / convergent.b;
    if (convergent.a > 0) {
      /* Check upper bound that numerator <= INT_MAX to prevent overflow. */
      const int upper_bound = (INT_MAX - prev_convergent.a) / convergent.a;
      if (n > upper_bound) { n = upper_bound; }
    }

    if (term >= options->max_terms || continued_fraction_term >= n) {
      break; /* Upper bound exceeded; truncate the continued fraction. */
    }

    const Fraction next_convergent = AppendContinuedFractionTerm(
        &convergent, &prev_convergent, (int)continued_fraction_term);
    prev_convergent = convergent;
    convergent = next_convergent;
  }

  Fraction best_approximation = convergent;
  /* We now possibly append a final term, if a semiconvergent would get a better
   * approximation than the last convergent.
   */
  const double lower_bound = continued_fraction_term / 2;
  /* [Rule 2] If n is less than continued_fraction_term / 2, then appending n
   * will get a worse approximation.
   */
  if (n >= lower_bound) {
    /* The last term must be reduced (rule 1), so don't exceed
     * continued_fraction_term. This happens if max_terms was exceeded.
     */
    if (n > continued_fraction_term) { n = (int)continued_fraction_term; }

    /* Append final continued fraction term. */
    const Fraction semiconvergent = AppendContinuedFractionTerm(
        &convergent, &prev_convergent, n);
    /* [Rule 3] A semiconvergent with n = continued_fraction_term / 2 might be a
     * worse approximation and must be tested vs. convergent.
     */
    if ((n > lower_bound) ||
        fabs(value - FractionToDouble(&semiconvergent)) <
            fabs(value - FractionToDouble(&convergent))) {
      best_approximation = semiconvergent; /* Accept the semiconvergent. */
    }
  }

  *out_numerator = sign * best_approximation.a;
  *out_denominator = best_approximation.b;
}
