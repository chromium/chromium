#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;
    use core::str;

    #[diplomat::opaque]
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
    }
}
