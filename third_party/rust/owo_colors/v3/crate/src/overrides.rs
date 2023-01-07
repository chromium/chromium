use core::sync::atomic::{AtomicU8, Ordering};

/// Set an override value for whether or not colors are supported.
///
/// If `true` is passed, [`if_supports_color`](crate::OwoColorize::if_supports_color) will always
/// act as if colors are supported.
///
/// If `false` is passed, [`if_supports_color`](crate::OwoColorize::if_supports_color) will always
/// act as if colors are **not** supported.
///
/// This behavior can be disabled using [`unset_override`], allowing `owo-colors` to return to
/// inferring if colors are supported.
#[cfg(feature = "supports-colors")]
pub fn set_override(enabled: bool) {
    OVERRIDE.set_force(enabled)
}

/// Remove any override value for whether or not colors are supported. This means
/// [`if_supports_color`](crate::OwoColorize::if_supports_color) will resume checking if the given
/// terminal output ([`Stream`](crate::Stream)) supports colors.
///
/// This override can be set using [`set_override`].
#[cfg(feature = "supports-colors")]
pub fn unset_override() {
    OVERRIDE.unset()
}

pub(crate) static OVERRIDE: Override = Override::none();

pub(crate) struct Override(AtomicU8);

const FORCE_MASK: u8 = 0b10;
const FORCE_ENABLE: u8 = 0b11;
const FORCE_DISABLE: u8 = 0b10;
const NO_FORCE: u8 = 0b00;

impl Override {
    const fn none() -> Self {
        Self(AtomicU8::new(NO_FORCE))
    }

    fn inner(&self) -> u8 {
        self.0.load(Ordering::SeqCst)
    }

    pub(crate) fn is_force_enabled_or_disabled(&self) -> (bool, bool) {
        let inner = self.inner();

        (inner == FORCE_ENABLE, inner == FORCE_DISABLE)
    }

    fn set_force(&self, enable: bool) {
        self.0.store(FORCE_MASK | (enable as u8), Ordering::SeqCst)
    }

    fn unset(&self) {
        self.0.store(0, Ordering::SeqCst);
    }
}
