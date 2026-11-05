//! Font model interop for Skrifa and HarfRust.
//!
//! This should not be used directly.

use crate::model::font::{Font, Tables};
use alloc::{boxed::Box, sync::Arc};
use core::any::Any;

/// Internal interop point for HarfRust.
///
/// Calling this directly will make your `Font` **UNUSABLE** with
/// HarfRust.
pub fn _get_or_init_shaping_data(
    font: &Font,
    f: impl FnOnce() -> Box<dyn Any + Send + Sync>,
) -> &dyn Any {
    &**font.shared().0.shaping_data.get_or_init(|| f())
}

/// Internal interop point for Skrifa.
///
/// Returns data that Skrifa derives from the font at its location. Every
/// clone of an instance shares the data. The slot for the default location
/// belongs to the font, so all default instances share it. A font without
/// variations is always at its default location, so it has one slot. Data
/// that is the same at every location belongs in the default location's
/// slot, which any instance can reach through [`Font::default_instance`].
///
/// Calling this directly will make your `Font` **UNUSABLE** with Skrifa.
pub fn _get_or_init_scaling_data(
    font: &Font,
    f: impl FnOnce() -> Box<dyn Any + Send + Sync>,
) -> &dyn Any {
    &**font.scaling_data().get_or_init(|| f())
}

/// Internal interop point for HarfRust.
///
/// Returns the shared tables of an SFNT font, for use as the owner of
/// cached data that borrows from them. Returns `None` for other fonts.
pub fn _table_cache_host(font: &Font) -> Option<Arc<Tables>> {
    font.shared().tables_arc().cloned()
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

    #[test]
    fn scaling_data_is_per_location() {
        struct Data(i32);
        let read = |data: &dyn Any| data.downcast_ref::<Data>().unwrap().0;
        let font = Font::new(font_test_data::VAZIRMATN_VAR, 0).unwrap();
        let bold = font
            .instance_builder()
            .variations([("wght", 700.0)])
            .build();
        let _ = _get_or_init_scaling_data(&font, || Box::new(Data(1)));
        // A varied instance has its own, which its clones share.
        assert_eq!(
            read(_get_or_init_scaling_data(&bold, || Box::new(Data(2)))),
            2
        );
        assert_eq!(
            read(_get_or_init_scaling_data(&bold.clone(), || Box::new(Data(
                3
            )))),
            2
        );
        // The default location's is the font's, reached from any instance.
        assert_eq!(
            read(_get_or_init_scaling_data(&bold.default_instance(), || {
                Box::new(Data(4))
            })),
            1
        );
        // A font without variations is always at its default location.
        let plain = Font::new(font_test_data::AHEM, 0).unwrap();
        let _ = _get_or_init_scaling_data(&plain, || Box::new(Data(5)));
        let asked = plain
            .instance_builder()
            .variations([("wght", 700.0)])
            .build();
        assert_eq!(
            read(_get_or_init_scaling_data(&asked, || Box::new(Data(6)))),
            5
        );
    }

    #[test]
    fn table_cache_host_shares_tables() {
        let font = Font::new(font_test_data::AHEM, 0).unwrap();
        let host = _table_cache_host(&font).unwrap();
        assert!(core::ptr::eq(&*host, font.tables()));
    }
}
