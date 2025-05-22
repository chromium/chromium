#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::duration::ffi::{Duration, TimeDuration};
    use crate::error::ffi::TemporalError;
    use crate::options::ffi::{DifferenceSettings, RoundingOptions};
    use alloc::boxed::Box;
    use alloc::string::String;
    use core::str::FromStr;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};

    #[cfg(feature = "compiled_data")]
    use crate::options::ffi::ToStringRoundingOptions;
    #[cfg(feature = "compiled_data")]
    use crate::time_zone::ffi::TimeZone;

    #[diplomat::opaque]
    pub struct Instant(pub temporal_rs::Instant);

    /// For portability, we use two i64s instead of an i128.
    /// The sign is extracted first before
    /// appending the high/low segments to each other.
    ///
    /// This could potentially instead be a bit-by-bit split, or something else
    pub struct I128Nanoseconds {
        pub high: i64,
        pub low: u64,
    }

    impl Instant {
        pub fn try_new(ns: I128Nanoseconds) -> Result<Box<Self>, TemporalError> {
            let is_neg = ns.high < 0;
            let ns_high_abs = ns.high.unsigned_abs() as u128;
            // Stick them together
            let total = ((ns_high_abs << 64) + ns.low as u128) as i128;
            // Reintroduce the sign
            let instant = if is_neg { -total } else { total };
            temporal_rs::Instant::try_new(instant)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_epoch_milliseconds(
            epoch_milliseconds: i64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Instant::from_epoch_milliseconds(epoch_milliseconds)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Instant::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::Instant::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn add(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn add_time_duration(
            &self,
            duration: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add_time_duration(&duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn subtract(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn subtract_time_duration(
            &self,
            duration: &TimeDuration,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract_time_duration(&duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .since(&other.0, settings.try_into()?)
                .map(|c| Box::new(Duration(c)))
                .map_err(Into::into)
        }
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .until(&other.0, settings.try_into()?)
                .map(|c| Box::new(Duration(c)))
                .map_err(Into::into)
        }
        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(options.try_into()?)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn epoch_milliseconds(&self) -> i64 {
            self.0.epoch_milliseconds()
        }

        pub fn epoch_nanoseconds(&self) -> I128Nanoseconds {
            let ns = self.0.epoch_nanoseconds().as_i128();
            let is_neg = ns < 0;
            let ns = ns.unsigned_abs();

            let high = (ns >> 64) as i64;
            let low = (ns & u64::MAX as u128) as u64;
            let high = if is_neg { -high } else { high };

            I128Nanoseconds { high, low }
        }

        #[cfg(feature = "compiled_data")]
        pub fn to_ixdtf_string_with_compiled_data(
            &self,
            zone: Option<&TimeZone>,
            options: ToStringRoundingOptions,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            use core::fmt::Write;
            let string = self.0.to_ixdtf_string(zone.map(|x| &x.0), options.into())?;
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);

            Ok(())
        }

        // TODO non-compiled data timezone APIs
    }
}
