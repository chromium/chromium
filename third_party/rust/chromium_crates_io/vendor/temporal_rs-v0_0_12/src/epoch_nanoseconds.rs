use crate::{error::ErrorMessage, TemporalError};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Default)]
pub struct EpochNanoseconds(pub(crate) i128);

impl From<i128> for EpochNanoseconds {
    fn from(value: i128) -> Self {
        Self(value)
    }
}

// Potential TODO: Build out primitive arthmetic methods if needed.
impl EpochNanoseconds {
    pub fn as_i128(&self) -> i128 {
        self.0
    }

    pub fn check_validity(&self) -> Result<(), TemporalError> {
        if !is_valid_epoch_nanos(&self.0) {
            return Err(TemporalError::range().with_enum(ErrorMessage::InstantOutOfRange));
        }
        Ok(())
    }
}

/// Utility for determining if the nanos are within a valid range.
#[inline]
#[must_use]
pub(crate) fn is_valid_epoch_nanos(nanos: &i128) -> bool {
    (crate::NS_MIN_INSTANT..=crate::NS_MAX_INSTANT).contains(nanos)
}
