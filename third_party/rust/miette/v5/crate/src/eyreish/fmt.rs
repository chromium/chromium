use super::{error::ErrorImpl, ptr::Ref};
use core::fmt;

impl ErrorImpl<()> {
    pub(crate) unsafe fn display(this: Ref<'_, Self>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        this.deref()
            .handler
            .as_ref()
            .map(|handler| handler.display(Self::error(this), f))
            .unwrap_or_else(|| core::fmt::Display::fmt(Self::diagnostic(this), f))
    }

    pub(crate) unsafe fn debug(this: Ref<'_, Self>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        this.deref()
            .handler
            .as_ref()
            .map(|handler| handler.debug(Self::diagnostic(this), f))
            .unwrap_or_else(|| core::fmt::Debug::fmt(Self::diagnostic(this), f))
    }
}
