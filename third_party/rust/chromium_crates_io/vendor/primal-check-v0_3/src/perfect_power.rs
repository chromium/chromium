use num_integer::Integer;

fn wrapping_pow(mut base: u64, mut exp: u32) -> u64 {
    let mut acc: u64 = 1;
    while exp > 0 {
        if exp % 2 == 1 {
            acc = acc.wrapping_mul(base)
        }
        base = base.wrapping_mul(base);
        exp /= 2;
    }
    acc
}

/// Returns integers `(y, k)` such that `x = y^k` with `k` maximised
/// (other than for `x = 0, 1`, in which case `y = x`, `k = 1`).
///
/// # Examples
///
/// ```rust
/// # use primal_check as primal;
/// assert_eq!(primal::as_perfect_power(2), (2, 1));
/// assert_eq!(primal::as_perfect_power(4), (2, 2));
/// assert_eq!(primal::as_perfect_power(8), (2, 3));
/// assert_eq!(primal::as_perfect_power(1024), (2, 10));
///
/// assert_eq!(primal::as_perfect_power(1000), (10, 3));
///
/// assert_eq!(primal::as_perfect_power(15), (15, 1));
/// ```
pub fn as_perfect_power(x: u64) -> (u64, u8) {
    if x == 0 || x == 1 {
        return (x, 1)
    }

    let floor_log_2 = 64 - x.leading_zeros() as u32 - 1;

    let x_ = x as f64;
    let mut last = (x, 1);
    // TODO: we could be smarter about this: we know all the possible
    // primes that can divide the exponent (since we have a list up to
    // 251 >= 64), so we really only need to check them.
    let mut expn: u32 = 2;
    let mut step = 1;
    while expn <= floor_log_2 {
        let factor = x_.powf(1.0/expn as f64).round() as u64;
        // the only case this will wrap is if x is close to 2^64 and
        // the round() rounds up, pushing this calculation over the
        // edge, however, the overflow will be well away from x, so we
        // still correctly don't take this branch. (x can't be a
        // perfect power if the result rounds away.)
        if wrapping_pow(factor, expn) == x {
            last = (factor, expn as u8);
            // if x is a 2nd and 5th power, it's going to be a 10th
            // power too, meaning we can search faster.
            // TODO: check if this is actually saving work
            step = step.lcm(&expn);
        }

        expn += step;
    }
    last
}

/// Return `Some((p, k))` if `x = p^k` for some prime `p` and `k >= 1`
/// (that is, including when `x` is itself a prime).
///
/// Returns `None` if `x` not a perfect power.
///
/// # Examples
///
/// ```rust
/// # use primal_check as primal;
/// assert_eq!(primal::as_prime_power(2), Some((2, 1)));
/// assert_eq!(primal::as_prime_power(4), Some((2, 2)));
/// assert_eq!(primal::as_prime_power(8), Some((2, 3)));
/// assert_eq!(primal::as_prime_power(1024), Some((2, 10)));
///
/// assert_eq!(primal::as_prime_power(1000), None);
///
/// assert_eq!(primal::as_prime_power(15), None);
/// ```
pub fn as_prime_power(x: u64) -> Option<(u64, u8)> {
    let (y, k) = as_perfect_power(x);
    if crate::miller_rabin(y) {
        Some((y, k))
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use primal::Sieve;

    use super::{as_perfect_power, as_prime_power};

    #[test]
    fn perfect_and_prime_power() {
        let tests = [
            (0, (0, 1), false),
            (1, (1, 1), false),
            (2, (2, 1), true),
            (3, (3, 1), true),
            (4, (2, 2), true),
            (5, (5, 1), true),
            (6, (6, 1), false),
            (8, (2, 3), true),
            (9, (3, 2), true),
            (16, (2, 4), true),
            (25, (5, 2), true),
            (32, (2, 5), true),
            (36, (6, 2), false),
            (100, (10, 2), false),
            (1000, (10, 3), false),
                ];

        for &(x, expected, is_prime) in tests.iter() {
            assert_eq!(as_perfect_power(x), expected);
            assert_eq!(as_prime_power(x),
                       if is_prime { Some(expected)} else { None })
        }

        let sieve = Sieve::new(200);
        let mut primes = sieve.primes_from(0);
        const MAX: f64 = 0xFFFF_FFFF_FFFF_FFFFu64 as f64;
        // test a whole pile of (semi)primes
        loop {
            let p = match primes.next() {
                Some(p) => p as u64,
                None => break
            };

            let subprimes = primes.clone().map(|x| (x, false));
            // include 1 to test p itself.
            for (q, is_prime) in Some((1, true)).into_iter().chain(subprimes) {
                let pq = p * q as u64;
                for n in 1..(MAX.log(pq as f64) as u32) {
                    let x = pq.pow(n);

                    let expected = (pq, n as u8);
                    assert_eq!(as_perfect_power(x), expected);
                    assert_eq!(as_prime_power(x),
                               if is_prime { Some(expected) } else { None });
                }
            }
        }
    }
}
