//! Variation axes and named instances.
//!
//! [`Font::axes`] returns the variation axes of a [`Font`], and
//! [`Font::named_instances`] returns its named instances. A [`Variation`] is
//! a value on one axis in user space. [`Font::instance_builder`] creates a
//! font at a location.

use super::Font;
use crate::{
    ps::type1::MultipleMaster,
    tables::fvar::{Fvar, VariationAxisRecord},
    types::{BigEndian, Fixed, NameId, Tag},
    FontData, FontRead, TableProvider,
};
use alloc::boxed::Box;
use core::str::FromStr;
use smallvec::SmallVec;

/// A normalized variation coordinate in 2.14 fixed point in the range
/// [-1.0, 1.0].
pub type NormalizedCoord = types::F2Dot14;

/// A variation setting for a font instance.
///
/// The tag identifies the axis, and the value is the desired value for that
/// axis in user space.
#[derive(Copy, Clone, PartialEq, Debug)]
pub struct Variation {
    /// The tag that identifies the axis.
    pub tag: Tag,
    /// The value for the axis in user space.
    pub value: f32,
}

impl Variation {
    /// Creates a new font variation with the given tag and value.
    pub fn new(tag: Tag, value: f32) -> Self {
        Self { tag, value }
    }
}

// Various conversions for Variation that have proven to be ergonomically
// useful in practice. These allow, for example, passing &[("wght", 700.0)]
// directly to the variations() method of InstanceBuilder without needing
// to manually construct Variation objects or tags.

impl From<&'_ Variation> for Variation {
    fn from(value: &'_ Variation) -> Self {
        *value
    }
}

impl From<(Tag, f32)> for Variation {
    fn from(value: (Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<&(Tag, f32)> for Variation {
    fn from(value: &(Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<(&str, f32)> for Variation {
    fn from(value: (&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

impl From<&(&str, f32)> for Variation {
    fn from(value: &(&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

/// A variation axis, such as weight or width, that a user can change in a
/// variable font.
///
/// See <https://fonts.google.com/knowledge/glossary/axis_in_variable_fonts>
#[derive(Copy, Clone, Debug)]
pub struct Axis {
    index: u32,
    record: VariationAxisRecord,
}

impl Axis {
    /// Returns the tag that identifies the axis.
    pub fn tag(&self) -> Tag {
        self.record.axis_tag()
    }

    /// Returns the index of the axis in the font's list of axes.
    pub fn index(&self) -> u32 {
        self.index
    }

    /// Returns the `name` table ID for the axis name.
    pub fn name(&self) -> NameId {
        self.record.axis_name_id()
    }

    /// Returns `true` if the axis should be hidden in user interfaces.
    pub fn is_hidden(&self) -> bool {
        const HIDDEN_AXIS: u16 = 0x1;
        self.record.flags() & HIDDEN_AXIS != 0
    }

    /// Returns the minimum value of the axis, in user space.
    pub fn min_value(&self) -> f32 {
        self.record.min_value().to_f32()
    }

    /// Returns the default value of the axis, in user space.
    pub fn default_value(&self) -> f32 {
        self.record.default_value().to_f32()
    }

    /// Returns the maximum value of the axis, in user space.
    pub fn max_value(&self) -> f32 {
        self.record.max_value().to_f32()
    }

    /// Clamps `value` to the range of the axis, as a font instance does.
    ///
    /// A maximum below the minimum is raised to it, as in normalization.
    pub(crate) fn clamp(&self, value: f32) -> f32 {
        let min = self.min_value();
        let max = self.max_value().max(min);
        value.max(min).min(max)
    }
}

/// A collection of variation axes.
///
/// This is empty if the font is not variable.
#[derive(Clone, Copy)]
pub struct Axes<'a> {
    records: &'a [VariationAxisRecord],
}

impl<'a> Axes<'a> {
    pub(crate) fn new(font: &'a Font) -> Self {
        let records = match font.type1_axes() {
            Some(records) => records,
            None => fvar(font)
                .and_then(|fvar| fvar.axes().ok())
                .unwrap_or_default(),
        };
        Self { records }
    }

    /// The number of axes.
    pub fn len(&self) -> u32 {
        self.records.len() as u32
    }

    /// Returns `true` if the font has no axes.
    pub fn is_empty(&self) -> bool {
        self.records.is_empty()
    }

    /// Returns the axis at `index`.
    pub fn get(&self, index: u32) -> Option<Axis> {
        let record = *self.records.get(index as usize)?;
        Some(Axis { index, record })
    }

    /// Returns the first axis with the given tag.
    pub fn get_by_tag(&self, tag: Tag) -> Option<Axis> {
        self.iter().find(|axis| axis.tag() == tag)
    }

    /// Returns the values in `variations` that apply to these axes.
    ///
    /// The rules are the same as those used when building a font instance:
    ///
    /// - A variation for an axis the font does not have is dropped.
    /// - If an axis is given more than once, the last value is used.
    /// - Each value is clamped to the range of its axis.
    ///
    /// The result is in the font's axis order and contains only the axes
    /// that were given.
    ///
    /// ```
    /// # use read_fonts::{model::{Font, Variation}, types::Tag};
    /// # fn wrapper(font: &Font) {
    /// // For a font with one `wght` axis from 100 to 900:
    /// let filtered: Vec<_> = font
    ///     .axes()
    ///     .filter([("wght", 400.0), ("opsz", 100.0), ("wght", 1200.0)])
    ///     .collect();
    /// assert_eq!(filtered, [Variation::new(Tag::new(b"wght"), 900.0)]);
    /// # }
    /// ```
    pub fn filter<V>(&self, variations: V) -> impl Iterator<Item = Variation> + Clone
    where
        V: IntoIterator,
        V::Item: Into<Variation>,
    {
        // One entry per axis, with the value given for it, if any.
        let mut values: SmallVec<[(Axis, Option<f32>); 8]> =
            self.iter().map(|axis| (axis, None)).collect();
        for variation in variations {
            let variation = variation.into();
            for (axis, value) in values.iter_mut() {
                if axis.tag() == variation.tag {
                    *value = Some(axis.clamp(variation.value));
                }
            }
        }
        values
            .into_iter()
            .filter_map(|(axis, value)| Some(Variation::new(axis.tag(), value?)))
    }

    /// Returns an iterator over the axes in font order.
    pub fn iter(&self) -> impl Iterator<Item = Axis> + 'a + Clone {
        self.records.iter().enumerate().map(|(index, record)| Axis {
            index: index as u32,
            record: *record,
        })
    }
}

/// A named location in a variable font, such as "Bold" or "Condensed
/// Light".
///
/// It has one user space value per axis. Use
/// [`InstanceBuilder::named_instance`](super::InstanceBuilder::named_instance)
/// to create a font at this location.
///
/// See <https://fonts.google.com/knowledge/glossary/instance>
#[derive(Clone, Copy)]
pub struct NamedInstance<'a> {
    index: u32,
    axes: &'a [VariationAxisRecord],
    coordinates: &'a [BigEndian<Fixed>],
    subfamily_name: NameId,
    postscript_name: Option<NameId>,
}

impl<'a> NamedInstance<'a> {
    /// Returns the index of the instance in the font's list of named
    /// instances.
    pub fn index(&self) -> u32 {
        self.index
    }

    /// Returns the `name` table ID for the instance's subfamily name.
    pub fn subfamily_name(&self) -> NameId {
        self.subfamily_name
    }

    /// Returns the `name` table ID for the instance's PostScript name, if it
    /// has one.
    pub fn postscript_name(&self) -> Option<NameId> {
        self.postscript_name
    }

    /// Returns the instance's value on each axis, in font axis order.
    pub fn user_coords(&self) -> impl Iterator<Item = f32> + 'a + Clone {
        self.coordinates.iter().map(|coord| coord.get().to_f32())
    }

    /// Returns the instance's value on each axis, paired with the axis tag.
    pub fn variations(&self) -> impl Iterator<Item = Variation> + 'a + Clone {
        self.axes
            .iter()
            .zip(self.user_coords())
            .map(|(axis, value)| Variation::new(axis.axis_tag(), value))
    }
}

/// A collection of named instances.
///
/// This is empty if the font is not variable.
#[derive(Clone, Copy)]
pub struct NamedInstances<'a> {
    /// The data of the `fvar` table, if the font has one.
    fvar: Option<FontData<'a>>,
}

impl<'a> NamedInstances<'a> {
    pub(crate) fn new(font: &'a Font) -> Self {
        Self {
            fvar: fvar(font).map(|fvar| fvar.offset_data()),
        }
    }

    /// The number of named instances.
    pub fn len(&self) -> u32 {
        self.fvar().map_or(0, |fvar| fvar.instance_count() as u32)
    }

    /// Returns `true` if the font has no named instances.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the named instance at `index`.
    pub fn get(&self, index: u32) -> Option<NamedInstance<'a>> {
        let fvar = self.fvar()?;
        let record = fvar.instances().ok()?.get(index as usize).ok()?;
        Some(NamedInstance {
            index,
            axes: fvar.axes().ok()?,
            coordinates: record.coordinates,
            subfamily_name: record.subfamily_name_id,
            postscript_name: record.post_script_name_id,
        })
    }

    /// Reads the `fvar` table from its data.
    fn fvar(&self) -> Option<Fvar<'a>> {
        Fvar::read(self.fvar?).ok()
    }

    /// Returns an iterator over the named instances in font order.
    pub fn iter(&self) -> impl Iterator<Item = NamedInstance<'a>> + 'a + Clone {
        let instances = *self;
        (0..self.len()).filter_map(move |index| instances.get(index))
    }
}

/// Returns the `fvar` table of an SFNT font.
///
/// Fonts of other kinds have no `fvar` table.
fn fvar(font: &Font) -> Option<Fvar<'_>> {
    font.tables().fvar().ok()
}

/// Returns axis records for the axes of a Type 1 multiple master font.
///
/// The values are design coordinates. An axis named `Weight`, `Width`,
/// `OpticalSize`, `Slant` or `Italic` takes the registered tag, as in
/// FreeType, and any other the tag `MMA` followed by its index. The axes
/// take name IDs from 256, which the font's names resolve to the axis names
/// in its dictionary.
pub(crate) fn type1_axis_records(mm: &MultipleMaster) -> Box<[VariationAxisRecord]> {
    let mut default = [Fixed::ZERO; 4];
    mm.default_blend(&mut default);
    mm.axes()
        .iter()
        .enumerate()
        .map(|(index, axis)| {
            let tag = match axis.name() {
                Some("Weight") => Tag::new(b"wght"),
                Some("Width") => Tag::new(b"wdth"),
                Some("OpticalSize") => Tag::new(b"opsz"),
                Some("Slant") => Tag::new(b"slnt"),
                Some("Italic") => Tag::new(b"ital"),
                _ => Tag::new(&[b'M', b'M', b'A', b'0' + index as u8]),
            };
            // Design coordinates beyond what a `Fixed` holds saturate.
            let design = |value: i32| BigEndian::from(Fixed::from_i32(value.clamp(-32768, 32767)));
            VariationAxisRecord {
                axis_tag: BigEndian::from(tag),
                min_value: design(axis.min_design()),
                default_value: design(axis.blend_to_design(default[index])),
                max_value: design(axis.max_design()),
                flags: BigEndian::from(0),
                axis_name_id: BigEndian::from(NameId::new(
                    super::name::TYPE1_AXIS_NAMES + index as u16,
                )),
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::VAZIRMATN_VAR;

    #[test]
    fn axes() {
        let font = Font::new(VAZIRMATN_VAR, 0).unwrap();
        let axes = font.axes();
        assert_eq!(axes.len(), 1);
        let axis = axes.get(0).unwrap();
        assert_eq!(axis.index(), 0);
        assert_eq!(axis.tag(), Tag::new(b"wght"));
        assert_eq!(
            (axis.min_value(), axis.default_value(), axis.max_value()),
            (100.0, 400.0, 900.0)
        );
        assert_eq!(axis.name(), NameId::new(257));
        assert!(!axis.is_hidden());
        assert_eq!(
            axes.get_by_tag(Tag::new(b"wght")).unwrap().index(),
            axis.index()
        );
        assert!(axes.get_by_tag(Tag::new(b"wdth")).is_none());
        assert!(axes.get(1).is_none());
        assert_eq!(axes.iter().count(), 1);
    }

    #[test]
    fn filter() {
        let font = Font::new(VAZIRMATN_VAR, 0).unwrap();
        // One wght axis from 100 to 900, with a default of 400.
        let axes = font.axes();
        let filter = |variations: &[(&str, f32)]| axes.filter(variations).collect::<Vec<_>>();
        // Axes the font does not have are dropped.
        assert_eq!(filter(&[("slnt", 25.0), ("wdth", 50.0)]), []);
        // Values are clamped to the axis.
        assert_eq!(filter(&[("wght", 50.0)]), [("wght", 100.0).into()]);
        assert_eq!(
            filter(&[("slnt", 25.0), ("wght", 1000.0)]),
            [("wght", 900.0).into()]
        );
        // The last value for an axis wins.
        assert_eq!(
            filter(&[("wght", 400.0), ("opsz", 100.0), ("wght", 120.5)]),
            [("wght", 120.5).into()]
        );
    }

    // Clamping agrees with normalization, which builds the instance, even
    // for an axis whose maximum is below its minimum.
    #[test]
    fn clamp_matches_normalization() {
        use crate::types::{BigEndian, Fixed};
        let axis = |min: f32, default: f32, max: f32| Axis {
            index: 0,
            record: VariationAxisRecord {
                axis_tag: BigEndian::from(Tag::new(b"wght")),
                min_value: BigEndian::from(Fixed::from_f64(min as f64)),
                default_value: BigEndian::from(Fixed::from_f64(default as f64)),
                max_value: BigEndian::from(Fixed::from_f64(max as f64)),
                flags: BigEndian::from(0),
                axis_name_id: BigEndian::from(NameId::new(256)),
            },
        };
        let inverted = axis(500.0, 400.0, 100.0);
        assert_eq!(inverted.clamp(300.0), 500.0);
        assert_eq!(inverted.clamp(700.0), 500.0);
        for axis in [inverted, axis(100.0, 400.0, 900.0)] {
            for value in [-1000.0, 0.0, 100.0, 300.0, 400.0, 500.0, 900.0, 2000.0] {
                let clamped = Fixed::from_f64(axis.clamp(value) as f64);
                let normalize = |value: Fixed| axis.record.normalize(value);
                assert_eq!(
                    normalize(clamped),
                    normalize(Fixed::from_f64(value as f64)),
                    "{value}"
                );
            }
        }
    }

    #[test]
    fn named_instances() {
        let font = Font::new(VAZIRMATN_VAR, 0).unwrap();
        let instances = font.named_instances();
        assert_eq!(instances.len() as usize, instances.iter().count());
        let bold = instances.get(6).unwrap();
        assert_eq!(bold.index(), 6);
        assert_eq!(bold.subfamily_name(), NameId::new(264));
        assert_eq!(bold.user_coords().collect::<Vec<_>>(), [700.0]);
        assert_eq!(
            bold.variations().collect::<Vec<_>>(),
            [Variation::new(Tag::new(b"wght"), 700.0)]
        );
        assert!(instances.get(instances.len()).is_none());
        // The same location the builder reaches by index.
        let by_index = font.instance_builder().named_instance(6).build();
        let by_variations = font
            .instance_builder()
            .variations(bold.variations())
            .build();
        assert_eq!(
            by_index.normalized_coords(),
            by_variations.normalized_coords()
        );
    }

    #[test]
    fn a_static_font_has_no_axes() {
        let font = Font::new(font_test_data::TINOS_SUBSET, 0).unwrap();
        assert!(font.axes().is_empty());
        assert!(font.named_instances().is_empty());
        assert!(font.named_instances().get(0).is_none());
    }
}
