#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;
    use core::fmt::Write;
    use core::str;
    use diplomat_runtime::DiplomatWrite;

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct TimeZone(pub temporal_rs::TimeZone);

    impl TimeZone {
        pub fn try_from_identifier_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            temporal_rs::TimeZone::try_from_identifier_str(ident)
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }
        pub fn try_from_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            temporal_rs::TimeZone::try_from_str(ident)
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }

        pub fn identifier(&self, write: &mut DiplomatWrite) -> Result<(), TemporalError> {
            // TODO ideally this would use Writeable instead of allocating
            let s = self.0.identifier()?;

            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = write.write_str(&s);

            Ok(())
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<TimeZone> {
            Box::new(TimeZone(self.0.clone()))
        }

        #[cfg(feature = "compiled_data")]
        pub fn is_valid(&self) -> bool {
            self.0.is_valid()
        }
    }
}
