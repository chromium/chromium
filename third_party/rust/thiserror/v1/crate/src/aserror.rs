use std::error::Error;
use std::panic::UnwindSafe;

pub trait AsDynError<'a> {
    fn as_dyn_error(&self) -> &(dyn Error + 'a);
}

impl<'a, T: Error + 'a> AsDynError<'a> for T {
    #[inline]
    fn as_dyn_error(&self) -> &(dyn Error + 'a) {
        self
    }
}

impl<'a> AsDynError<'a> for dyn Error + 'a {
    #[inline]
    fn as_dyn_error(&self) -> &(dyn Error + 'a) {
        self
    }
}

impl<'a> AsDynError<'a> for dyn Error + Send + 'a {
    #[inline]
    fn as_dyn_error(&self) -> &(dyn Error + 'a) {
        self
    }
}

impl<'a> AsDynError<'a> for dyn Error + Send + Sync + 'a {
    #[inline]
    fn as_dyn_error(&self) -> &(dyn Error + 'a) {
        self
    }
}

impl<'a> AsDynError<'a> for dyn Error + Send + Sync + UnwindSafe + 'a {
    #[inline]
    fn as_dyn_error(&self) -> &(dyn Error + 'a) {
        self
    }
}
