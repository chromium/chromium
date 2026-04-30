fn mod_mul_(a: u64, b: u64, m: u64) -> u64 {
    (u128::from(a) * u128::from(b) % u128::from(m)) as u64
}

fn mod_mul(a: u64, b: u64, m: u64) -> u64 {
    match a.checked_mul(b) {
        Some(r) => if r >= m { r % m } else { r },
        None => mod_mul_(a, b, m),
    }
}

fn mod_sqr(a: u64, m: u64) -> u64 {
    if a < (1 << 32) {
        let r = a * a;
        if r >= m {
            r % m
        } else {
            r
        }
    } else {
        mod_mul_(a, a, m)
    }
}

fn mod_exp(mut x: u64, mut d: u64, n: u64) -> u64 {
    let mut ret: u64 = 1;
    while d != 0 {
        if d % 2 == 1 {
            ret = mod_mul(ret, x, n)
        }
        d /= 2;
        x = mod_sqr(x, n);
    }
    ret
}

/// Test if `n` is prime, using the deterministic version of the
/// Miller-Rabin test.
///
/// Doing a lot of primality tests with numbers strictly below some
/// upper bound will be faster using the `is_prime` method of a
/// `Sieve` instance.
///
/// # Examples
///
/// ```rust
/// assert_eq!(primal::is_prime(1), false);
/// assert_eq!(primal::is_prime(2), true);
/// assert_eq!(primal::is_prime(3), true);
/// assert_eq!(primal::is_prime(4), false);
/// assert_eq!(primal::is_prime(5), true);
///
/// assert_eq!(primal::is_prime(22_801_763_487), false);
/// assert_eq!(primal::is_prime(22_801_763_489), true);
/// assert_eq!(primal::is_prime(22_801_763_491), false);
/// ```
pub fn miller_rabin(n: u64) -> bool {
    const HINT: &[u64] = &[2];

    // we have a strict upper bound, so we can just use the witness
    // table of Pomerance, Selfridge & Wagstaff and Jeaschke to be as
    // efficient as possible, without having to fall back to
    // randomness. Additional limits from Feitsma and Galway complete
    // the entire range of `u64`. See also:
    // https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test#Testing_against_small_sets_of_bases
    const WITNESSES: &[(u64, &[u64])] = &[
        (2_046, HINT),
        (1_373_652, &[2, 3]),
        (9_080_190, &[31, 73]),
        (25_326_000, &[2, 3, 5]),
        (4_759_123_140, &[2, 7, 61]),
        (1_112_004_669_632, &[2, 13, 23, 1662803]),
        (2_152_302_898_746, &[2, 3, 5, 7, 11]),
        (3_474_749_660_382, &[2, 3, 5, 7, 11, 13]),
        (341_550_071_728_320, &[2, 3, 5, 7, 11, 13, 17]),
        (3_825_123_056_546_413_050, &[2, 3, 5, 7, 11, 13, 17, 19, 23]),
        (std::u64::MAX, &[2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]),
    ];

    if n % 2 == 0 { return n == 2 }
    if n == 1 { return false }

    let mut d = n - 1;
    let mut s = 0;
    while d % 2 == 0 { d /= 2; s += 1 }

    let witnesses =
        WITNESSES.iter().find(|&&(hi, _)| hi >= n)
            .map(|&(_, wtnss)| wtnss).unwrap();
    'next_witness: for &a in witnesses.iter() {
        let mut power = mod_exp(a, d, n);
        assert!(power < n);
        if power == 1 || power == n - 1 { continue 'next_witness }

        for _r in 0..s {
            power = mod_sqr(power, n);
            assert!(power < n);
            if power == 1 { return false }
            if power == n - 1 {
                continue 'next_witness
            }
        }
        return false
    }

    true
}

#[cfg(test)]
mod tests {
    use primal::Sieve;

    #[test]
    fn mod_mul() {
        assert_eq!(super::mod_mul(1 << 63, 1 << 32, 3), 2);
        assert_eq!(super::mod_mul(1 << 31, 1 << 31, (1 << 32) - 7), 3221225479);
        assert_eq!(super::mod_mul(1 << 32, 1 << 32, (1 << 32) - 7), 49);
        assert_eq!(super::mod_mul(1 << 32, 1 << 32, (1 << 32) + 7), 49);
        assert_eq!(super::mod_mul(1 << 63, 1 << 32, (1 << 32) + 7), 2_147_483_480);
        assert_eq!(super::mod_mul(1 << 63, 1 << 32, (1 << 63) + 7), 9_223_372_006_790_004_743);
        assert_eq!(super::mod_mul(1 << 32, 1 << 32, !0), 1);
    }

    #[test]
    fn miller_rabin() {
        const LIMIT: usize = 1_000_000;
        let sieve = Sieve::new(LIMIT);
        for x in 0..LIMIT {
            let s = sieve.is_prime(x);
            let mr = super::miller_rabin(x as u64);

            assert!(s == mr, "miller_rabin {} mismatches sieve {} for {}",
                    mr, s, x)
        }
    }
    #[test]
    fn miller_rabin_large() {
        let tests = &[
            (4_294_967_311, true),
            (4_294_967_291, true),
            (4_294_967_291 * 4_294_967_291, false),
            (!0, false),
            ];
        for &(n, is_prime) in tests {
            assert!(super::miller_rabin(n) == is_prime,
                    "mismatch for {} (should be {})", n, is_prime);
        }
    }

    #[test]
    fn oeis_a014233() {
        // https://oeis.org/A014233
        const A014233: [u64; 9] = [
            2047,
            1373653,
            25326001,
            3215031751,
            2152302898747,
            3474749660383,
            341550071728321,
            341550071728321,
            3825123056546413051,
            // 3825123056546413051,
            // 3825123056546413051,
            // 318665857834031151167461,
            // 3317044064679887385961981,
        ];
        for &n in &A014233 {
            assert!(!super::miller_rabin(n), "{} is composite!", n);
        }
    }
}
