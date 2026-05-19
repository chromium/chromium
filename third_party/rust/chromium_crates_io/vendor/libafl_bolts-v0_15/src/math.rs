//! Math-related functions that we commonly (or at least sometimes) need

use core::ops::AddAssign;

use crate::Error;

/// Returns the cumulative distribution function for a discrete distribution.
pub fn calculate_cumulative_distribution_in_place(probabilities: &mut [f32]) -> Result<(), Error> {
    if probabilities.is_empty() {
        return Err(Error::illegal_argument("empty list of probabilities"));
    }

    if !probabilities.iter().all(|&p| (0.0..=1.0).contains(&p)) {
        return Err(Error::illegal_argument(format!(
            "invalid probability distribution: {probabilities:?}"
        )));
    }

    let cumulative = probabilities;
    calculate_cumulative_sum_in_place(cumulative);

    // The cumulative sum should be roughly equal to 1.
    let last = cumulative.last_mut().unwrap();
    let offset_to_1 = *last - 1.0_f32;
    if offset_to_1.is_nan() || offset_to_1 > 1.0e-4 || -offset_to_1 > 1.0e-4 {
        return Err(Error::illegal_argument(format!(
            "sum of probabilities ({last}) is not 1"
        )));
    }

    // Clamp the end of the vector to account for floating point errors.
    *last = f32::INFINITY;

    Ok(())
}

/// Calculates the integer square root using binary search
/// Algorithm from
/// <https://en.wikipedia.org/wiki/Integer_square_root#Algorithm_using_binary_search>.
#[must_use]
pub const fn integer_sqrt(val: u64) -> u64 {
    if val == u64::MAX {
        return 2_u64.pow(32) - 1;
    }
    let mut ret = 0;
    let mut i = val + 1;
    let mut m;

    while ret != i - 1 {
        m = u64::midpoint(ret, i);

        if m.saturating_mul(m) <= val {
            ret = m;
        } else {
            i = m;
        }
    }

    ret
}

/// Calculates the cumulative sum for a slice, in-place.
/// The values are useful for example for cumulative probabilities.
///
/// So, to give an example:
/// ```rust
/// # extern crate libafl_bolts;
/// use libafl_bolts::math::calculate_cumulative_sum_in_place;
///
/// let mut value = [2, 4, 1, 3];
/// calculate_cumulative_sum_in_place(&mut value);
/// assert_eq!(&[2, 6, 7, 10], &value);
/// ```
pub fn calculate_cumulative_sum_in_place<T>(mut_slice: &mut [T])
where
    T: Default + AddAssign<T> + Copy,
{
    let mut acc = T::default();

    for val in mut_slice {
        acc += *val;
        *val = acc;
    }
}

#[cfg(test)]
mod test {
    use super::{calculate_cumulative_distribution_in_place, integer_sqrt};

    #[test]
    fn test_integer_sqrt() {
        assert_eq!(0, integer_sqrt(0));
        assert_eq!(1, integer_sqrt(1));
        assert_eq!(2, integer_sqrt(4));
        assert_eq!(10, integer_sqrt(120));
        assert_eq!(11, integer_sqrt(121));
        assert_eq!(11, integer_sqrt(128));
        assert_eq!(2_u64.pow(16) - 1, integer_sqrt(u64::from(u32::MAX)));
        assert_eq!(2_u64.pow(32) - 1, integer_sqrt(u64::MAX));
        assert_eq!(2_u64.pow(32) - 1, integer_sqrt(u64::MAX - 1));
        assert_eq!(128, integer_sqrt(128 * 128));
        assert_eq!(128, integer_sqrt((128 * 128) + 1));
        assert_eq!(128, integer_sqrt((128 * 128) + 127));
        assert_eq!(128, integer_sqrt((128 * 128) + 127));
        assert_eq!(999999, integer_sqrt((999999 * 999999) + 9));
    }

    #[test]
    fn get_cdf_fails_with_invalid_probs() {
        // Distribution has to sum up to 1.
        assert!(calculate_cumulative_distribution_in_place(&mut []).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.0]).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.9]).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.9, 0.9]).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [f32::NAN]).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [f32::INFINITY]).is_err());
        assert!(calculate_cumulative_distribution_in_place(&mut [f32::NEG_INFINITY]).is_err());

        // Elements have to be between 0 and 1
        assert!(calculate_cumulative_distribution_in_place(&mut [-0.5, 0.5, 0.5]).is_err());

        assert!(calculate_cumulative_distribution_in_place(&mut [1.0]).is_ok());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.0, 1.0]).is_ok());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.0, 1.0, 0.0]).is_ok());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.5, 0.5]).is_ok());
        assert!(calculate_cumulative_distribution_in_place(&mut [0.2; 5]).is_ok());
    }
}
