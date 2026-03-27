//! Model for PostScript fonts.

mod cff;
#[cfg(feature = "std")]
mod type1;

pub use cff::{CffFontRef, CffSubfont};
#[cfg(feature = "std")]
pub use type1::Type1Font;

use super::dict::Blues;
use types::Fixed;

/// Parameters used to generate the stem and counter zones for the hinting
/// algorithm.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct HintingParams {
    pub blues: Blues,
    pub family_blues: Blues,
    pub other_blues: Blues,
    pub family_other_blues: Blues,
    pub blue_scale: Fixed,
    pub blue_shift: Fixed,
    pub blue_fuzz: Fixed,
    pub language_group: i32,
}

impl Default for HintingParams {
    fn default() -> Self {
        Self {
            blues: Blues::default(),
            other_blues: Blues::default(),
            family_blues: Blues::default(),
            family_other_blues: Blues::default(),
            // See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-16-private-dict-operators>
            blue_scale: Fixed::from_f64(0.039625),
            blue_shift: Fixed::from_i32(7),
            blue_fuzz: Fixed::ONE,
            language_group: 0,
        }
    }
}
