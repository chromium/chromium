//! Hinting parameters.

use types::Fixed;

/// Maximum number of values in a set of blue alignment zones.
// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psblues.h#L141>
pub const MAX_BLUE_VALUES: usize = 7;

/// Operand for the `BlueValues`, `OtherBlues`, `FamilyBlues` and
/// `FamilyOtherBlues` operators.
///
/// These are used to generate zones when applying hints.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct Blues {
    values: [(Fixed, Fixed); MAX_BLUE_VALUES],
    len: u32,
}

impl Blues {
    /// Creates a new set of blues from the given values.
    pub fn new(values: impl Iterator<Item = Fixed>) -> Self {
        let mut blues = Self::default();
        let mut stash = Fixed::ZERO;
        for (i, value) in values.take(MAX_BLUE_VALUES * 2).enumerate() {
            if (i & 1) == 0 {
                stash = value;
            } else {
                blues.values[i / 2] = (stash, value);
                blues.len += 1;
            }
        }
        blues
    }

    /// Returns the blue value regions.
    pub fn values(&self) -> &[(Fixed, Fixed)] {
        &self.values[..self.len as usize]
    }
}

/// Maximum number of stem snap parameters.
// Summary: older PostScript interpreters accept two values, but newer ones
// accept 12. We'll assume that as maximum.
// <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5049.StemSnap.pdf>
pub const MAX_STEM_SNAPS: usize = 12;

/// Operand for the `StemSnapH` and `StemSnapV` operators.
///
/// These are used for stem darkening when applying hints.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct StemSnaps {
    values: [Fixed; MAX_STEM_SNAPS],
    len: u32,
}

impl StemSnaps {
    /// Creates a new set of stem snaps for the given values.
    pub fn new(values: impl Iterator<Item = Fixed>) -> Self {
        let mut snaps = Self::default();
        for (value, target_value) in values.take(MAX_STEM_SNAPS).zip(&mut snaps.values) {
            *target_value = value;
            snaps.len += 1;
        }
        snaps
    }

    /// Returns the stem snap values.
    pub fn values(&self) -> &[Fixed] {
        &self.values[..self.len as usize]
    }
}

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
