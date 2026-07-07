//! Font model interop for Skrifa and HarfRust.
//!
//! This should not be used directly.

use crate::model::font::Font;
use alloc::boxed::Box;
use core::any::Any;

/// Internal interop point for HarfRust.
///
/// Calling this directly will make your `Font` **UNUSABLE** with
/// HarfRust.
pub fn _get_or_init_shaping_data(
    font: &Font,
    f: impl FnOnce() -> Box<dyn Any + Send + Sync>,
) -> &dyn Any {
    &**font.0.shaping_data.get_or_init(|| f())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recover_shaping_data() {
        struct ShapingData {
            arg: i32,
        }
        let font = Font::new(font_test_data::AHEM, 0).unwrap();
        let data = _get_or_init_shaping_data(&font, || Box::new(ShapingData { arg: -42 }));
        let actual = data.downcast_ref::<ShapingData>().unwrap();
        assert_eq!(actual.arg, -42);
    }
}
