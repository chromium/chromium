use num_traits::{One, PrimInt, Zero};

pub fn primitive_root(prime: u64) -> Option<u64> {
    let test_exponents: Vec<u64> = distinct_prime_factors(prime - 1)
        .iter()
        .map(|factor| (prime - 1) / factor)
        .collect();
    'next: for potential_root in 2..prime {
        // for each distinct factor, if potential_root^(p-1)/factor mod p is 1, reject it
        for exp in &test_exponents {
            if modular_exponent(potential_root, *exp, prime) == 1 {
                continue 'next;
            }
        }

        // if we reach this point, it means this root was not rejected, so return it
        return Some(potential_root);
    }
    None
}

/// computes base^exponent % modulo using the standard exponentiation by squaring algorithm
pub fn modular_exponent<T: PrimInt>(mut base: T, mut exponent: T, modulo: T) -> T {
    let one = T::one();

    let mut result = one;

    while exponent > Zero::zero() {
        if exponent & one == one {
            result = result * base % modulo;
        }
        exponent = exponent >> One::one();
        base = (base * base) % modulo;
    }

    result
}

/// return all of the prime factors of n, but omit duplicate prime factors
pub fn distinct_prime_factors(mut n: u64) -> Vec<u64> {
    let mut result = Vec::new();

    // handle 2 separately so we dont have to worry about adding 2 vs 1
    if n % 2 == 0 {
        while n % 2 == 0 {
            n /= 2;
        }
        result.push(2);
    }
    if n > 1 {
        let mut divisor = 3;
        let mut limit = (n as f32).sqrt() as u64 + 1;
        while divisor < limit {
            if n % divisor == 0 {
                // remove as many factors as possible from n
                while n % divisor == 0 {
                    n /= divisor;
                }
                result.push(divisor);

                // recalculate the limit to reduce the amount of work we need to do
                limit = (n as f32).sqrt() as u64 + 1;
            }

            divisor += 2;
        }

        if n > 1 {
            result.push(n);
        }
    }

    result
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct PrimeFactor {
    pub value: usize,
    pub count: u32,
}

#[derive(Clone, Debug)]
pub struct PrimeFactors {
    other_factors: Vec<PrimeFactor>,
    n: usize,
    power_two: u32,
    power_three: u32,
    total_factor_count: u32,
    distinct_factor_count: u32,
}
impl PrimeFactors {
    pub fn compute(mut n: usize) -> Self {
        let mut result = Self {
            other_factors: Vec::new(),
            n,
            power_two: 0,
            power_three: 0,
            total_factor_count: 0,
            distinct_factor_count: 0,
        };

        // compute powers of two separately
        result.power_two = n.trailing_zeros();
        result.total_factor_count += result.power_two;
        n >>= result.power_two;
        if result.power_two > 0 {
            result.distinct_factor_count += 1;
        }

        // also compute powers of three separately
        while n % 3 == 0 {
            result.power_three += 1;
            n /= 3;
        }
        result.total_factor_count += result.power_three;
        if result.power_three > 0 {
            result.distinct_factor_count += 1;
        }

        // if we have any other factors, gather them in the "other factors" vec
        if n > 1 {
            let mut divisor = 5;
            // compute divisor limit. if our divisor goes above this limit, we know we won't find any more factors. we'll revise it downwards as we discover factors.
            let mut limit = (n as f32).sqrt() as usize + 1;
            while divisor < limit {
                // Count how many times this divisor divesthe remaining input
                let mut count = 0;
                while n % divisor == 0 {
                    n /= divisor;
                    count += 1;
                }

                // If this entry is actually a divisor of the given number, add it to the array
                if count > 0 {
                    result.other_factors.push(PrimeFactor {
                        value: divisor,
                        count,
                    });
                    result.total_factor_count += count;
                    result.distinct_factor_count += 1;

                    // recalculate the limit to reduce the amount of other factors we need to check
                    limit = (n as f32).sqrt() as usize + 1;
                }

                divisor += 2;
            }

            // because of our limit logic, there might be one factor left
            if n > 1 {
                result
                    .other_factors
                    .push(PrimeFactor { value: n, count: 1 });
                result.total_factor_count += 1;
                result.distinct_factor_count += 1;
            }
        }

        result
    }

    pub fn is_prime(&self) -> bool {
        self.total_factor_count == 1
    }
    pub fn get_product(&self) -> usize {
        self.n
    }
    #[allow(unused)]
    pub fn get_total_factor_count(&self) -> u32 {
        self.total_factor_count
    }
    #[allow(unused)]
    pub fn get_distinct_factor_count(&self) -> u32 {
        self.distinct_factor_count
    }
    #[allow(unused)]
    pub fn get_power_of_two(&self) -> u32 {
        self.power_two
    }
    #[allow(unused)]
    pub fn get_power_of_three(&self) -> u32 {
        self.power_three
    }
    #[allow(unused)]
    pub fn get_other_factors(&self) -> &[PrimeFactor] {
        &self.other_factors
    }
    #[allow(unused)]
    pub fn is_power_of_three(&self) -> bool {
        self.power_three > 0 && self.power_two == 0 && self.other_factors.len() == 0
    }

    // Divides the number by the given prime factor. Returns None if the resulting number is one.
    #[allow(unused)]
    pub fn remove_factors(mut self, factor: PrimeFactor) -> Option<Self> {
        if factor.count == 0 {
            return Some(self);
        }
        if factor.value == 2 {
            self.power_two = self.power_two.checked_sub(factor.count).unwrap();
            self.n >>= factor.count;
            self.total_factor_count -= factor.count;
            if self.power_two == 0 {
                self.distinct_factor_count -= 1;
            }
            if self.n > 1 {
                return Some(self);
            }
        } else if factor.value == 3 {
            self.power_three = self.power_three.checked_sub(factor.count).unwrap();
            self.n /= 3.pow(factor.count);
            self.total_factor_count -= factor.count;
            if self.power_two == 0 {
                self.distinct_factor_count -= 1;
            }
            if self.n > 1 {
                return Some(self);
            }
        } else {
            let found_factor = self
                .other_factors
                .iter_mut()
                .find(|item| item.value == factor.value)
                .unwrap();
            found_factor.count = found_factor.count.checked_sub(factor.count).unwrap();
            self.n /= factor.value.pow(factor.count);
            self.total_factor_count -= factor.count;
            if found_factor.count == 0 {
                self.distinct_factor_count -= 1;
                self.other_factors.retain(|item| item.value != factor.value);
            }
            if self.n > 1 {
                return Some(self);
            }
        }
        None
    }

    // returns true if we have any factors whose value is less than or equal to the provided factor
    pub fn has_factors_leq(&self, factor: usize) -> bool {
        self.power_two > 0
            || self.power_three > 0
            || self
                .other_factors
                .first()
                .map_or(false, |f| f.value <= factor)
    }

    // returns true if we have any factors whose value is greater than the provided factor
    pub fn has_factors_gt(&self, factor: usize) -> bool {
        (factor < 2 && self.power_two > 0)
            || (factor < 3 && self.power_three > 0)
            || self
                .other_factors
                .last()
                .map_or(false, |f| f.value > factor)
    }

    // returns the product of all factors greater than the provided min_factor
    pub fn product_above(&self, min_factor: usize) -> usize {
        self.other_factors
            .iter()
            .skip_while(|f| f.value <= min_factor)
            .map(|f| f.value.pow(f.count))
            .product()
    }

    // Splits this set of prime factors into two different sets so that the products of the two sets are as close as possible
    pub fn partition_factors(mut self) -> (Self, Self) {
        // Make sure this isn't a prime number
        assert!(!self.is_prime());

        // If the given length is a perfect square, put the square root into both returned arays
        if self.power_two % 2 == 0
            && self.power_three % 2 == 0
            && self
                .other_factors
                .iter()
                .all(|factor| factor.count % 2 == 0)
        {
            let mut new_product = 1;

            // cut our power of two in half
            self.power_two /= 2;
            new_product <<= self.power_two;

            // cout our power of three in half
            self.power_three /= 2;
            new_product *= 3.pow(self.power_three);

            // cut all our other factors in half
            for factor in self.other_factors.iter_mut() {
                factor.count /= 2;
                new_product *= factor.value.pow(factor.count);
            }

            // update our cached properties and return 2 copies of ourself
            self.total_factor_count /= 2;
            self.n = new_product;
            (self.clone(), self)
        } else if self.distinct_factor_count == 1 {
            // If there's only one factor, just split it as evenly as possible
            let mut half = Self {
                other_factors: Vec::new(),
                n: self.n,
                power_two: self.power_two / 2,
                power_three: self.power_three / 2,
                total_factor_count: self.total_factor_count / 2,
                distinct_factor_count: 1,
            };

            // We computed one half via integer division -- compute the other half by subtracting the divided values fro mthe original
            self.power_two -= half.power_two;
            self.power_three -= half.power_three;
            self.total_factor_count -= half.total_factor_count;

            // Update the product values for each half, with different logic depending on what kind of single factor we have
            if let Some(first_factor) = self.other_factors.first_mut() {
                // we actualyl skipped updating the "other factor"  earlier, so cut it in half and do the subtraction now
                assert!(first_factor.count > 1); // If this is only one, then we're prime. we passed the "is_prime" assert earlier, so that would be a contradiction
                let half_factor = PrimeFactor {
                    value: first_factor.value,
                    count: first_factor.count / 2,
                };
                first_factor.count -= half_factor.count;
                half.other_factors.push(half_factor);

                self.n = first_factor.value.pow(first_factor.count);
                half.n = half_factor.value.pow(half_factor.count);
            } else if half.power_two > 0 {
                half.n = 1 << half.power_two;
                self.n = 1 << self.power_two;
            } else if half.power_three > 0 {
                half.n = 3.pow(half.power_three);
                self.n = 3.pow(self.power_three);
            }

            (self, half)
        } else {
            // we have a mixed bag of products. we're going to greedily try to evenly distribute entire groups of factors in one direction or the other
            let mut left_product = 1;
            let mut right_product = 1;

            // for each factor, put it in whichever cumulative half is smaller
            for factor in self.other_factors {
                let factor_product = factor.value.pow(factor.count as u32);
                if left_product <= right_product {
                    left_product *= factor_product;
                } else {
                    right_product *= factor_product;
                }
            }
            if left_product <= right_product {
                left_product <<= self.power_two;
            } else {
                right_product <<= self.power_two;
            }
            if self.power_three > 0 && left_product <= right_product {
                left_product *= 3.pow(self.power_three);
            } else {
                right_product *= 3.pow(self.power_three);
            }

            // now that we have our two products, compute a prime factorization for them
            // we could maintain factor lists internally to save some computation and an allocation, but it led to a lot of code and this is so much simpler
            (Self::compute(left_product), Self::compute(right_product))
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub struct PartialFactors {
    power2: u32,
    power3: u32,
    power5: u32,
    power7: u32,
    power11: u32,
    other_factors: usize,
}
impl PartialFactors {
    #[allow(unused)]
    pub fn compute(len: usize) -> Self {
        let power2 = len.trailing_zeros();
        let mut other_factors = len >> power2;

        let mut power3 = 0;
        while other_factors % 3 == 0 {
            power3 += 1;
            other_factors /= 3;
        }

        let mut power5 = 0;
        while other_factors % 5 == 0 {
            power5 += 1;
            other_factors /= 5;
        }

        let mut power7 = 0;
        while other_factors % 7 == 0 {
            power7 += 1;
            other_factors /= 7;
        }

        let mut power11 = 0;
        while other_factors % 11 == 0 {
            power11 += 1;
            other_factors /= 11;
        }

        Self {
            power2,
            power3,
            power5,
            power7,
            power11,
            other_factors,
        }
    }

    #[allow(unused)]
    pub fn get_power2(&self) -> u32 {
        self.power2
    }
    #[allow(unused)]
    pub fn get_power3(&self) -> u32 {
        self.power3
    }
    #[allow(unused)]
    pub fn get_power5(&self) -> u32 {
        self.power5
    }
    #[allow(unused)]
    pub fn get_power7(&self) -> u32 {
        self.power7
    }
    #[allow(unused)]
    pub fn get_power11(&self) -> u32 {
        self.power11
    }
    #[allow(unused)]
    pub fn get_other_factors(&self) -> usize {
        self.other_factors
    }
    #[allow(unused)]
    pub fn product(&self) -> usize {
        (self.other_factors
            * 3.pow(self.power3)
            * 5.pow(self.power5)
            * 7.pow(self.power7)
            * 11.pow(self.power11))
            << self.power2
    }
    #[allow(unused)]
    pub fn product_power2power3(&self) -> usize {
        3.pow(self.power3) << self.power2
    }
    #[allow(unused)]
    pub fn divide_by(&self, divisor: &PartialFactors) -> Option<PartialFactors> {
        let two_divides = self.power2 >= divisor.power2;
        let three_divides = self.power3 >= divisor.power3;
        let five_divides = self.power5 >= divisor.power5;
        let seven_divides = self.power7 >= divisor.power7;
        let eleven_divides = self.power11 >= divisor.power11;
        let other_divides = self.other_factors % divisor.other_factors == 0;
        if two_divides
            && three_divides
            && five_divides
            && seven_divides
            && eleven_divides
            && other_divides
        {
            Some(Self {
                power2: self.power2 - divisor.power2,
                power3: self.power3 - divisor.power3,
                power5: self.power5 - divisor.power5,
                power7: self.power7 - divisor.power7,
                power11: self.power11 - divisor.power11,
                other_factors: if self.other_factors == divisor.other_factors {
                    1
                } else {
                    self.other_factors / divisor.other_factors
                },
            })
        } else {
            None
        }
    }
}

#[cfg(test)]
mod unit_tests {
    use super::*;

    #[test]
    fn test_modular_exponent() {
        // make sure to test something that would overflow under ordinary circumstances
        // ie 3 ^ 416788 mod 47
        let test_list = vec![
            ((2, 8, 300), 256),
            ((2, 9, 300), 212),
            ((1, 9, 300), 1),
            ((3, 416788, 47), 8),
        ];

        for (input, expected) in test_list {
            let (base, exponent, modulo) = input;

            let result = modular_exponent(base, exponent, modulo);

            assert_eq!(result, expected);
        }
    }

    #[test]
    fn test_primitive_root() {
        let test_list = vec![(3, 2), (7, 3), (11, 2), (13, 2), (47, 5), (7919, 7)];

        for (input, expected) in test_list {
            let root = primitive_root(input).unwrap();

            assert_eq!(root, expected);
        }
    }

    #[test]
    fn test_distinct_prime_factors() {
        let test_list = vec![
            (46, vec![2, 23]),
            (2, vec![2]),
            (3, vec![3]),
            (162, vec![2, 3]),
        ];

        for (input, expected) in test_list {
            let factors = distinct_prime_factors(input);

            assert_eq!(factors, expected);
        }
    }

    use std::collections::HashMap;

    macro_rules! map{
        { $($key:expr => $value:expr),+ } => {
            {
                let mut m = HashMap::new();
                $(
                    m.insert($key, $value);
                )+
                m
            }
         };
    }

    fn assert_internally_consistent(prime_factors: &PrimeFactors) {
        let mut cumulative_product = 1;
        let mut discovered_distinct_factors = 0;
        let mut discovered_total_factors = 0;

        if prime_factors.get_power_of_two() > 0 {
            cumulative_product <<= prime_factors.get_power_of_two();
            discovered_distinct_factors += 1;
            discovered_total_factors += prime_factors.get_power_of_two();
        }
        if prime_factors.get_power_of_three() > 0 {
            cumulative_product *= 3.pow(prime_factors.get_power_of_three());
            discovered_distinct_factors += 1;
            discovered_total_factors += prime_factors.get_power_of_three();
        }
        for factor in prime_factors.get_other_factors() {
            assert!(factor.count > 0);
            cumulative_product *= factor.value.pow(factor.count);
            discovered_distinct_factors += 1;
            discovered_total_factors += factor.count;
        }

        assert_eq!(prime_factors.get_product(), cumulative_product);
        assert_eq!(
            prime_factors.get_distinct_factor_count(),
            discovered_distinct_factors
        );
        assert_eq!(
            prime_factors.get_total_factor_count(),
            discovered_total_factors
        );
        assert_eq!(prime_factors.is_prime(), discovered_total_factors == 1);
    }

    #[test]
    fn test_prime_factors() {
        #[derive(Debug)]
        struct ExpectedData {
            len: usize,
            factors: HashMap<usize, u32>,
            total_factors: u32,
            distinct_factors: u32,
            is_prime: bool,
        }
        impl ExpectedData {
            fn new(
                len: usize,
                factors: HashMap<usize, u32>,
                total_factors: u32,
                distinct_factors: u32,
                is_prime: bool,
            ) -> Self {
                Self {
                    len,
                    factors,
                    total_factors,
                    distinct_factors,
                    is_prime,
                }
            }
        }

        let test_list = vec![
            ExpectedData::new(2, map! { 2 => 1 }, 1, 1, true),
            ExpectedData::new(128, map! { 2 => 7 }, 7, 1, false),
            ExpectedData::new(3, map! { 3 => 1 }, 1, 1, true),
            ExpectedData::new(81, map! { 3 => 4 }, 4, 1, false),
            ExpectedData::new(5, map! { 5 => 1 }, 1, 1, true),
            ExpectedData::new(125, map! { 5 => 3 }, 3, 1, false),
            ExpectedData::new(97, map! { 97 => 1 }, 1, 1, true),
            ExpectedData::new(6, map! { 2 => 1, 3 => 1 }, 2, 2, false),
            ExpectedData::new(12, map! { 2 => 2, 3 => 1 }, 3, 2, false),
            ExpectedData::new(36, map! { 2 => 2, 3 => 2 }, 4, 2, false),
            ExpectedData::new(10, map! { 2 => 1, 5 => 1 }, 2, 2, false),
            ExpectedData::new(100, map! { 2 => 2, 5 => 2 }, 4, 2, false),
            ExpectedData::new(44100, map! { 2 => 2, 3 => 2, 5 => 2, 7 => 2 }, 8, 4, false),
        ];

        for expected in test_list {
            let factors = PrimeFactors::compute(expected.len);

            assert_eq!(factors.get_product(), expected.len);
            assert_eq!(factors.is_prime(), expected.is_prime);
            assert_eq!(
                factors.get_distinct_factor_count(),
                expected.distinct_factors
            );
            assert_eq!(factors.get_total_factor_count(), expected.total_factors);
            assert_eq!(
                factors.get_power_of_two(),
                expected.factors.get(&2).map_or(0, |i| *i)
            );
            assert_eq!(
                factors.get_power_of_three(),
                expected.factors.get(&3).map_or(0, |i| *i)
            );

            // verify that every factor in the "other factors" array matches our expected map
            for factor in factors.get_other_factors() {
                assert_eq!(factor.count, *expected.factors.get(&factor.value).unwrap());
            }

            // finally, verify that every factor in the "other factors" array was present in the "other factors" array
            let mut found_factors: Vec<usize> = factors
                .get_other_factors()
                .iter()
                .map(|factor| factor.value)
                .collect();
            if factors.get_power_of_two() > 0 {
                found_factors.push(2);
            }
            if factors.get_power_of_three() > 0 {
                found_factors.push(3);
            }
            for key in expected.factors.keys() {
                assert!(found_factors.contains(key as &usize));
            }
        }

        // in addition to our precomputed list, go through a bunch of ofther factors and just make sure they're internally consistent
        for n in 1..200 {
            let factors = PrimeFactors::compute(n);
            assert_eq!(factors.get_product(), n);

            assert_internally_consistent(&factors);
        }
    }

    #[test]
    fn test_partition_factors() {
        // We aren't going to verify the actual return value of "partition_factors", we're justgoing to make sure each half is internally consistent
        for n in 4..200 {
            let factors = PrimeFactors::compute(n);
            if !factors.is_prime() {
                let (left_factors, right_factors) = factors.partition_factors();

                assert!(left_factors.get_product() > 1);
                assert!(right_factors.get_product() > 1);

                assert_eq!(left_factors.get_product() * right_factors.get_product(), n);

                assert_internally_consistent(&left_factors);
                assert_internally_consistent(&right_factors);
            }
        }
    }

    #[test]
    fn test_remove_factors() {
        // For every possible factor of a bunch of factors, they removing each and making sure the result is internally consistent
        for n in 2..200 {
            let factors = PrimeFactors::compute(n);

            for i in 0..=factors.get_power_of_two() {
                if let Some(removed_factors) = factors
                    .clone()
                    .remove_factors(PrimeFactor { value: 2, count: i })
                {
                    assert_eq!(removed_factors.get_product(), factors.get_product() >> i);
                    assert_internally_consistent(&removed_factors);
                } else {
                    // If the method returned None, this must be a power of two and i must be equal to the product
                    assert!(n.is_power_of_two());
                    assert!(i == factors.get_power_of_two());
                }
            }
        }
    }

    #[test]
    fn test_partial_factors() {
        #[derive(Debug)]
        struct ExpectedData {
            len: usize,
            power2: u32,
            power3: u32,
            power5: u32,
            power7: u32,
            power11: u32,
            other: usize,
        }

        let test_list = vec![
            ExpectedData {
                len: 2,
                power2: 1,
                power3: 0,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 128,
                power2: 7,
                power3: 0,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 3,
                power2: 0,
                power3: 1,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 81,
                power2: 0,
                power3: 4,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 5,
                power2: 0,
                power3: 0,
                power5: 1,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 125,
                power2: 0,
                power3: 0,
                power5: 3,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 97,
                power2: 0,
                power3: 0,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 97,
            },
            ExpectedData {
                len: 6,
                power2: 1,
                power3: 1,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 12,
                power2: 2,
                power3: 1,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 36,
                power2: 2,
                power3: 2,
                power5: 0,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 10,
                power2: 1,
                power3: 0,
                power5: 1,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 100,
                power2: 2,
                power3: 0,
                power5: 2,
                power7: 0,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 44100,
                power2: 2,
                power3: 2,
                power5: 2,
                power7: 2,
                power11: 0,
                other: 1,
            },
            ExpectedData {
                len: 2310,
                power2: 1,
                power3: 1,
                power5: 1,
                power7: 1,
                power11: 1,
                other: 1,
            },
        ];

        for expected in test_list {
            let factors = PartialFactors::compute(expected.len);

            assert_eq!(factors.get_power2(), expected.power2);
            assert_eq!(factors.get_power3(), expected.power3);
            assert_eq!(factors.get_power5(), expected.power5);
            assert_eq!(factors.get_power7(), expected.power7);
            assert_eq!(factors.get_power11(), expected.power11);
            assert_eq!(factors.get_other_factors(), expected.other);

            assert_eq!(
                expected.len,
                (1 << factors.get_power2())
                    * 3.pow(factors.get_power3())
                    * 5.pow(factors.get_power5())
                    * 7.pow(factors.get_power7())
                    * 11.pow(factors.get_power11())
                    * factors.get_other_factors()
            );
            assert_eq!(expected.len, factors.product());
            assert_eq!(
                (1 << factors.get_power2()) * 3.pow(factors.get_power3()),
                factors.product_power2power3()
            );
            assert_eq!(factors.get_other_factors().trailing_zeros(), 0);
            assert!(factors.get_other_factors() % 3 > 0);
        }

        // in addition to our precomputed list, go through a bunch of ofther factors and just make sure they're internally consistent
        for n in 1..200 {
            let factors = PartialFactors::compute(n);

            assert_eq!(
                n,
                (1 << factors.get_power2())
                    * 3.pow(factors.get_power3())
                    * 5.pow(factors.get_power5())
                    * 7.pow(factors.get_power7())
                    * 11.pow(factors.get_power11())
                    * factors.get_other_factors()
            );
            assert_eq!(n, factors.product());
            assert_eq!(
                (1 << factors.get_power2()) * 3.pow(factors.get_power3()),
                factors.product_power2power3()
            );
            assert_eq!(factors.get_other_factors().trailing_zeros(), 0);
            assert!(factors.get_other_factors() % 3 > 0);
        }
    }

    #[test]
    fn test_partial_factors_divide_by() {
        for n in 2..200 {
            let factors = PartialFactors::compute(n);

            for power2 in 0..5 {
                for power3 in 0..4 {
                    for power5 in 0..3 {
                        for power7 in 0..3 {
                            for power11 in 0..2 {
                                for power13 in 0..2 {
                                    let divisor_product = (3.pow(power3)
                                        * 5.pow(power5)
                                        * 7.pow(power7)
                                        * 11.pow(power11)
                                        * 13.pow(power13))
                                        << power2;
                                    let divisor = PartialFactors::compute(divisor_product);
                                    if let Some(quotient) = factors.divide_by(&divisor) {
                                        assert_eq!(quotient.product(), n / divisor_product);
                                    } else {
                                        assert!(n % divisor_product > 0);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
