//! Helpers for selecting a font size and location in variation space.

use read_fonts::types::Fixed;

use crate::small_array::SmallArray;

/// Type for a normalized variation coordinate.
pub type NormalizedCoord = read_fonts::types::F2Dot14;

/// Font size in pixels per em units.
///
/// Sizes in this crate are represented as a ratio of pixels to the size of
/// the em square defined by the font. This is equivalent to the `px` unit
/// in CSS (assuming a DPI scale factor of 1.0).
///
/// To retrieve metrics and outlines in font units, use the [unscaled](Self::unscaled)
/// construtor on this type.
#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
pub struct Size(f32);

impl Size {
    /// Creates a new font size from the given value in pixels per em units.
    ///
    /// Providing a value `<= 0.0` is equivalent to creating an unscaled size
    /// and will result in metrics and outlines generated in font units.
    pub fn new(ppem: f32) -> Self {
        Self(ppem)
    }

    /// Creates a new font size for generating unscaled metrics or outlines in
    /// font units.
    pub fn unscaled() -> Self {
        Self(0.0)
    }

    /// Returns the raw size in pixels per em units.
    ///
    /// Results in `None` if the size is unscaled.
    pub fn ppem(self) -> Option<f32> {
        (self.0 > 0.0).then_some(self.0)
    }

    /// Computes a linear scale factor for this font size and the given units
    /// per em value which can be retrieved from the [Metrics](crate::metrics::Metrics)
    /// type or from the [head](read_fonts::tables::head::Head) table.
    ///
    /// Returns 1.0 for an unscaled size or when `units_per_em` is 0.
    pub fn linear_scale(self, units_per_em: u16) -> f32 {
        if self.0 > 0.0 && units_per_em != 0 {
            self.0 / units_per_em as f32
        } else {
            1.0
        }
    }

    /// Computes a fixed point linear scale factor that matches FreeType.
    pub(crate) fn fixed_linear_scale(self, units_per_em: u16) -> Fixed {
        // FreeType computes a 16.16 scale factor that converts to 26.6.
        // This is done in two steps, assuming use of FT_Set_Pixel_Size:
        // 1) height is multiplied by 64:
        //    <https://gitlab.freedesktop.org/freetype/freetype/-/blob/49781ab72b2dfd0f78172023921d08d08f323ade/src/base/ftobjs.c#L3596>
        // 2) this value is divided by UPEM:
        //    (here, scaled_h=height and h=upem)
        //    <https://gitlab.freedesktop.org/freetype/freetype/-/blob/49781ab72b2dfd0f78172023921d08d08f323ade/src/base/ftobjs.c#L3312>
        if self.0 > 0.0 && units_per_em > 0 {
            Fixed::from_bits((self.0 * 64.) as i32) / Fixed::from_bits(units_per_em as i32)
        } else {
            // This is an identity scale for the pattern
            // `mul_div(value, scale, 64)`
            Fixed::from_bits(0x10000 * 64)
        }
    }
}

/// Reference to an ordered sequence of normalized variation coordinates.
///
/// This type represents a position in the variation space where each
/// coordinate corresponds to an axis (in the same order as the `fvar` table)
/// and is a normalized value in the range `[-1..1]`.
///
/// See [Coordinate Scales and Normalization](https://learn.microsoft.com/en-us/typography/opentype/spec/otvaroverview#coordinate-scales-and-normalization)
/// for further details.
///
/// If the array is larger in length than the number of axes, extraneous
/// values are ignored. If it is smaller, unrepresented axes are assumed to be
/// at their default positions (i.e. 0).
///
/// A value of this type constructed with `default()` represents the default
/// position for each axis.
///
/// Normalized coordinates are ignored for non-variable fonts.
#[derive(Copy, Clone, Default, Debug)]
pub struct LocationRef<'a>(&'a [NormalizedCoord]);

impl<'a> LocationRef<'a> {
    /// Creates a new sequence of normalized coordinates from the given array.
    pub fn new(coords: &'a [NormalizedCoord]) -> Self {
        Self(coords)
    }

    /// Returns the underlying array of normalized coordinates.
    pub fn coords(&self) -> &'a [NormalizedCoord] {
        self.0
    }
}

impl<'a> From<&'a [NormalizedCoord]> for LocationRef<'a> {
    fn from(value: &'a [NormalizedCoord]) -> Self {
        Self(value)
    }
}

impl<'a> IntoIterator for LocationRef<'a> {
    type IntoIter = core::slice::Iter<'a, NormalizedCoord>;
    type Item = &'a NormalizedCoord;

    fn into_iter(self) -> Self::IntoIter {
        self.0.iter()
    }
}

impl<'a> IntoIterator for &'_ LocationRef<'a> {
    type IntoIter = core::slice::Iter<'a, NormalizedCoord>;
    type Item = &'a NormalizedCoord;

    fn into_iter(self) -> Self::IntoIter {
        self.0.iter()
    }
}

/// Maximum number of coords to store inline in a `Location` object.
///
/// This value was chosen to maximize use of space in the underlying
/// `SmallArray` storage.
const MAX_INLINE_COORDS: usize = 8;

/// Ordered sequence of normalized variation coordinates.
///
/// This is an owned version of [`LocationRef`]. See the documentation on that
/// type for more detail.
#[derive(Clone, Debug)]
pub struct Location {
    coords: SmallArray<NormalizedCoord, MAX_INLINE_COORDS>,
}

impl Location {
    /// Creates a new location with the given number of normalized coordinates.
    ///
    /// Each element will be initialized to the default value (0.0).
    pub fn new(len: usize) -> Self {
        Self {
            coords: SmallArray::new(NormalizedCoord::default(), len),
        }
    }

    /// Returns the underlying slice of normalized coordinates.
    pub fn coords(&self) -> &[NormalizedCoord] {
        self.coords.as_slice()
    }

    /// Returns a mutable reference to the underlying slice of normalized
    /// coordinates.
    pub fn coords_mut(&mut self) -> &mut [NormalizedCoord] {
        self.coords.as_mut_slice()
    }
}

impl Default for Location {
    fn default() -> Self {
        Self {
            coords: SmallArray::new(NormalizedCoord::default(), 0),
        }
    }
}

impl<'a> From<&'a Location> for LocationRef<'a> {
    fn from(value: &'a Location) -> Self {
        LocationRef(value.coords())
    }
}

impl<'a> IntoIterator for &'a Location {
    type IntoIter = core::slice::Iter<'a, NormalizedCoord>;
    type Item = &'a NormalizedCoord;

    fn into_iter(self) -> Self::IntoIter {
        self.coords().iter()
    }
}

impl<'a> IntoIterator for &'a mut Location {
    type IntoIter = core::slice::IterMut<'a, NormalizedCoord>;
    type Item = &'a mut NormalizedCoord;

    fn into_iter(self) -> Self::IntoIter {
        self.coords_mut().iter_mut()
    }
}
