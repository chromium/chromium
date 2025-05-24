#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    #[diplomat::enum_convert(temporal_rs::error::ErrorKind)]
    pub enum ErrorKind {
        Generic,
        Type,
        Range,
        Syntax,
        Assert,
    }

    // In the future we might turn this into an opaque type with a msg() field
    pub struct TemporalError {
        pub kind: ErrorKind,
    }

    impl TemporalError {
        // internal
        pub(crate) fn syntax() -> Self {
            TemporalError {
                kind: ErrorKind::Syntax,
            }
        }

        pub(crate) fn range() -> Self {
            TemporalError {
                kind: ErrorKind::Range,
            }
        }
    }
}

impl From<temporal_rs::TemporalError> for ffi::TemporalError {
    fn from(other: temporal_rs::TemporalError) -> Self {
        Self {
            kind: other.kind().into(),
        }
    }
}
