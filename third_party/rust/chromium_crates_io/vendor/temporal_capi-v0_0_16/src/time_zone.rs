#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use crate::provider::ffi::Provider;
    use alloc::boxed::Box;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatWrite;

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    pub struct TimeZone(pub temporal_rs::TimeZone);

    impl TimeZone {
        #[cfg(feature = "compiled_data")]
        pub fn try_from_identifier_str(ident: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            Self::try_from_identifier_str_with_provider(ident, &Provider::compiled())
        }
        pub fn try_from_identifier_str_with_provider<'p>(
            ident: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            with_provider!(p, |p| {
                temporal_rs::TimeZone::try_from_identifier_str_with_provider(ident, p)
            })
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
            Self::try_from_str_with_provider(ident, &Provider::compiled())
        }
        pub fn try_from_str_with_provider<'p>(
            ident: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            with_provider!(p, |p| temporal_rs::TimeZone::try_from_str_with_provider(
                ident, p
            ))
            .map(|x| Box::new(TimeZone(x)))
            .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn identifier(&self, write: &mut DiplomatWrite) {
            let _ = self.identifier_with_provider(&Provider::compiled(), write);
        }

        pub fn identifier_with_provider<'p>(
            &self,
            p: &Provider<'p>,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO ideally this would use Writeable instead of allocating
            let s = with_provider!(p, |p| self.0.identifier_with_provider(p)).unwrap_or_default();

            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = write.write_str(&s);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        pub fn utc() -> Box<Self> {
            // TODO merge signature with below
            Box::new(Self(temporal_rs::TimeZone::utc()))
        }

        /// Create a TimeZone that represents +00:00
        ///
        /// This is the only way to infallibly make a TimeZone without compiled_data,
        /// and can be used as a fallback.
        pub fn zero() -> Box<Self> {
            // TODO merge signature with below
            Box::new(Self(temporal_rs::TimeZone::UtcOffset(Default::default())))
        }

        pub fn utc_with_provider<'p>(p: &Provider<'p>) -> Result<Box<Self>, TemporalError> {
            Ok(Box::new(Self(with_provider!(p, |p| {
                temporal_rs::TimeZone::utc_with_provider(p)
            }))))
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<TimeZone> {
            Box::new(TimeZone(self.0))
        }

        /// Get the primary time zone identifier corresponding to this time zone
        #[cfg(feature = "compiled_data")]
        pub fn primary_identifier(&self) -> Result<Box<Self>, TemporalError> {
            self.primary_identifier_with_provider(&Provider::compiled())
        }
        pub fn primary_identifier_with_provider<'p>(
            &self,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.primary_identifier_with_provider(p))
                .map(|x| Box::new(TimeZone(x)))
                .map_err(Into::into)
        }
    }
}
