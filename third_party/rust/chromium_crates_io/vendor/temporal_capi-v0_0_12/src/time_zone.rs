#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatWrite;

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct TimeZone(pub temporal_rs::TimeZone);

    impl TimeZone {
        #[cfg(feature = "compiled_data")]
        pub fn try_from_identifier_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            temporal_rs::TimeZone::try_from_identifier_str(ident)
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }
        pub fn try_from_offset_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::UtcOffset::from_utf8(ident)
                .map(|x| Box::new(TimeZone(temporal_rs::TimeZone::UtcOffset(x))))
                .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn try_from_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            temporal_rs::TimeZone::try_from_str(ident)
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }

        pub fn identifier(&self, write: &mut DiplomatWrite) {
            // TODO ideally this would use Writeable instead of allocating
            let s = self.0.identifier();

            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = write.write_str(&s);
        }

        pub fn utc() -> Box<Self> {
            Box::new(Self(temporal_rs::TimeZone::IanaIdentifier("UTC".into())))
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<TimeZone> {
            Box::new(TimeZone(self.0.clone()))
        }

        /// Get the primary time zone identifier corresponding to this time zone
        #[cfg(feature = "compiled_data")]
        pub fn primary_identifier(&self) -> Result<Box<Self>, TemporalError> {
            self.0
                .primary_identifier()
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }

        // To be removed on any release after 0.0.11
        #[cfg(feature = "compiled_data")]
        pub fn is_valid(&self) -> bool {
            true
        }
    }
}
