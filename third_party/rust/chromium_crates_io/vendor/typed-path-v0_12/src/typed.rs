/// Implements the body of a function where we map
/// to the potential path types and call their
/// underlying function.
macro_rules! impl_typed_fn {
    ($self:ident, $f:ident $(, $($tts:tt)*)?) => {
        match $self {
            Self::Unix(this) => this.$f($($($tts)*)?),
            Self::Windows(this) => this.$f($($($tts)*)?),
        }
    };
}

mod non_utf8;
mod utf8;

pub use non_utf8::*;
pub use utf8::*;

/// Represents the type of the path.
pub enum PathType {
    /// Path is for a Unix platform.
    Unix,

    /// Path is for a Windows platform.
    Windows,
}
