use num_traits::FromPrimitive;

use crate::{TemporalError, NS_MAX_INSTANT};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct EpochNanoseconds(pub(crate) i128);

impl TryFrom<i128> for EpochNanoseconds {
    type Error = TemporalError;
    fn try_from(value: i128) -> Result<Self, Self::Error> {
        if !is_valid_epoch_nanos(&value) {
            return Err(TemporalError::range()
                .with_message("Instant nanoseconds are not within a valid epoch range."));
        }
        Ok(Self(value))
    }
}

impl TryFrom<u128> for EpochNanoseconds {
    type Error = TemporalError;
    fn try_from(value: u128) -> Result<Self, Self::Error> {
        if (NS_MAX_INSTANT as u128) < value {
            return Err(TemporalError::range()
                .with_message("Instant nanoseconds are not within a valid epoch range."));
        }
        Ok(Self(value as i128))
    }
}

impl TryFrom<f64> for EpochNanoseconds {
    type Error = TemporalError;
    fn try_from(value: f64) -> Result<Self, Self::Error> {
        let Some(value) = i128::from_f64(value) else {
            return Err(TemporalError::range()
                .with_message("Instant nanoseconds are not within a valid epoch range."));
        };
        Self::try_from(value)
    }
}

// Potential TODO: Build out primitive arthmetic methods if needed.
impl EpochNanoseconds {
    pub fn as_i128(&self) -> i128 {
        self.0
    }
}

/// Utility for determining if the nanos are within a valid range.
#[inline]
#[must_use]
pub(crate) fn is_valid_epoch_nanos(nanos: &i128) -> bool {
    (crate::NS_MIN_INSTANT..=crate::NS_MAX_INSTANT).contains(nanos)
}
