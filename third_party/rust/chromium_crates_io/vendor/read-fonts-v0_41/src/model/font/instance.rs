//! Font instance representation.

use super::Font;
use crate::{
    tables::{
        avar::Avar,
        fvar::Fvar,
        layout::{Condition, FeatureVariations},
    },
    TableProvider,
};
use alloc::vec::Vec;
use core::{
    str::FromStr,
    sync::atomic::{self, AtomicU32},
};
use types::{Fixed, Tag};

/// A specific instance of a font, with a size and variation settings.
pub struct FontInstance {
    font: Font,
    size: Option<f32>,
    coords: CoordStorage,
    feature_vars: FeatureVarsStorage,
}

impl FontInstance {
    /// Returns a builder for configuring a font instance from the given font.
    pub fn builder(font: &Font) -> FontInstanceBuilder {
        FontInstanceBuilder {
            instance: Self {
                font: font.clone(),
                size: None,
                coords: CoordStorage::default(),
                feature_vars: FeatureVarsStorage::new(),
            },
        }
    }

    /// Returns the font for this instance.
    pub fn font(&self) -> &Font {
        &self.font
    }

    /// Returns the size of the font instance, in pixels per em.
    pub fn size(&self) -> Option<f32> {
        self.size
    }

    /// Returns the normalized variation coordinates for this font instance.
    pub fn normalized_coords(&self) -> &[NormalizedCoord] {
        self.coords.as_slice()
    }

    /// Returns the selected feature variations for this font instance.
    pub fn feature_variations(&self) -> FontFeatureVariations {
        self.feature_vars.load(&self.font, self.coords.as_slice())
    }
}

impl core::ops::Deref for FontInstance {
    type Target = Font;
    fn deref(&self) -> &Font {
        self.font()
    }
}

/// Builder for configuring a font instance.
pub struct FontInstanceBuilder {
    instance: FontInstance,
}

impl FontInstanceBuilder {
    /// Sets the size for the font instance, in pixels per em.
    ///
    /// Setting this to `None` disables scaling.
    pub fn size(mut self, size: Option<f32>) -> Self {
        self.instance.size = size;
        self
    }

    /// Sets the variations for the font instance from an unordered sequence of
    /// variations in user space.
    ///
    /// Omitted axes will be set to their default values. Unsupported axes are
    /// ignored. If an axis is specified multiple times, the last value is used.
    ///
    /// This will overwrite any previous variation settings.
    pub fn variations<V>(mut self, variations: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<FontVariation>,
    {
        self.set_variations(variations);
        self
    }

    /// Sets the variations for the font instance from an ordered sequence
    /// of normalized coordinates.
    ///
    /// If the number of provided coordinates is less than the number of axes,
    /// the remaining axes will be set to their default values. If the number
    /// of provided coordinates is greater than the number of axes, the extra
    /// coordinates will be ignored.
    ///
    /// This will overwrite any previous variation settings.
    pub fn normalized_coords(mut self, coords: impl IntoIterator<Item = NormalizedCoord>) -> Self {
        self.set_coords(coords);
        self
    }

    /// Sets the variations for the font instance from a named instance.
    ///
    /// If the given named instance index is invalid, then variation settings
    /// will be reset to default.
    ///
    /// This will overwrite any previous variation settings.
    pub fn named_instance(mut self, index: usize) -> Self {
        self.set_named_instance(index);
        self
    }

    /// Sets the variations for the font instance from a named instance, with
    /// additional overrides.
    ///
    /// If the given named instance index is invalid, then it is ignored and
    /// only overrides are applied.
    ///
    /// This will overwrite any previous variation settings.
    pub fn named_instance_with_overrides<V>(mut self, index: usize, overrides: V) -> Self
    where
        V: IntoIterator,
        V::Item: Into<FontVariation>,
    {
        self.set_named_instance_with_overrides(index, overrides);
        self
    }

    /// Builds the font instance.
    pub fn build(self) -> FontInstance {
        self.instance
    }
}

impl FontInstanceBuilder {
    fn set_variations<V>(&mut self, variations: V)
    where
        V: IntoIterator,
        V::Item: Into<FontVariation>,
    {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                variations,
            );
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_coords(&mut self, coords: impl IntoIterator<Item = NormalizedCoord>) {
        if let Ok(fvar) = self.instance.font.tables().fvar() {
            let count = fvar.axis_count() as usize;
            self.instance.coords.resize(count);
            for (dst, src) in self.instance.coords.as_mut_slice().iter_mut().zip(
                coords
                    .into_iter()
                    .chain(core::iter::repeat(NormalizedCoord::ZERO)),
            ) {
                *dst = src;
            }
            self.instance.coords.clear_if_all_zeroes();
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_named_instance(&mut self, index: usize) {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                named_instance_variations(&fvar, index),
            );
        } else {
            self.instance.coords.resize(0);
        }
    }

    fn set_named_instance_with_overrides<V>(&mut self, index: usize, overrides: V)
    where
        V: IntoIterator,
        V::Item: Into<FontVariation>,
    {
        let tables = self.instance.font.tables();
        if let Ok(fvar) = tables.fvar() {
            set_variations(
                &fvar,
                tables.avar().ok(),
                &mut self.instance.coords,
                named_instance_variations(&fvar, index)
                    .chain(overrides.into_iter().map(Into::into)),
            );
        } else {
            self.instance.coords.resize(0);
        }
    }
}

// Helper to extract an iterator of FontVariation from a named instance index.
fn named_instance_variations<'a>(
    fvar: &'a Fvar,
    index: usize,
) -> impl Iterator<Item = FontVariation> + 'a {
    fvar.axis_instance_arrays()
        .ok()
        .and_then(|arrays| {
            let axes = arrays.axes();
            arrays.instances().get(index).ok().map(|instance| {
                axes.iter()
                    .zip(instance.coordinates)
                    .map(|(axis, coord)| FontVariation::new(axis.axis_tag(), coord.get().to_f32()))
            })
        })
        .into_iter()
        .flatten()
}

/// Helper for setting variations.
///
/// Pulled out into a separate function to avoid borrow checker issues.
fn set_variations<V>(fvar: &Fvar, avar: Option<Avar>, coords: &mut CoordStorage, variations: V)
where
    V: IntoIterator,
    V::Item: Into<FontVariation>,
{
    coords.resize(fvar.axis_count() as usize);
    fvar.user_to_normalized(
        avar.as_ref(),
        variations
            .into_iter()
            .map(Into::into)
            .map(|var| (var.tag, Fixed::from_f64(var.value as _))),
        coords.as_mut_slice(),
    );
    coords.clear_if_all_zeroes();
}

/// A normalized variation coordinate in 2.14 fixed point in the range
/// [-1.0, 1.0].
pub type NormalizedCoord = types::F2Dot14;

/// A variation setting for a font instance.
///
/// The tag identifies the axis, and the value is the desired value for that
/// axis in user space.
#[derive(Copy, Clone, PartialEq, Debug)]
pub struct FontVariation {
    /// The tag that identifies the axis.
    pub tag: Tag,
    /// The value for the axis in user space.
    pub value: f32,
}

impl FontVariation {
    /// Creates a new font variation with the given tag and value.
    pub fn new(tag: Tag, value: f32) -> Self {
        Self { tag, value }
    }
}

// Various conversions for FontVariation that have proven to be ergonomically
// useful in practice. These allow, for example, passing &[("wght", 700.0)]
// directly to the variations() method of FontInstanceBuilder without needing
//to manually construct FontVariation objects or tags.

impl From<&'_ FontVariation> for FontVariation {
    fn from(value: &'_ FontVariation) -> Self {
        *value
    }
}

impl From<(Tag, f32)> for FontVariation {
    fn from(value: (Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<&(Tag, f32)> for FontVariation {
    fn from(value: &(Tag, f32)) -> Self {
        Self::new(value.0, value.1)
    }
}

impl From<(&str, f32)> for FontVariation {
    fn from(value: (&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

impl From<&(&str, f32)> for FontVariation {
    fn from(value: &(&str, f32)) -> Self {
        Self::new(Tag::from_str(value.0).unwrap_or_default(), value.1)
    }
}

/// Maximum number of coordinates we store inline. Chosen to maximize
/// number of coords while minimizing space overhead.
const MAX_INLINE_COORDS: usize = 15;

enum CoordStorage {
    None,
    Inline([NormalizedCoord; MAX_INLINE_COORDS], u8),
    Heap(Vec<NormalizedCoord>),
}

impl Default for CoordStorage {
    fn default() -> Self {
        Self::None
    }
}

impl CoordStorage {
    /// Empty storage if all the coordinates are zeros. This allows us to
    /// bypass variation processing for the default instance with a simple
    /// is_empty() check.
    fn clear_if_all_zeroes(&mut self) {
        match self {
            Self::None => {}
            Self::Inline(coords, len) => {
                if coords[..*len as usize]
                    .iter()
                    .all(|&c| c == NormalizedCoord::ZERO)
                {
                    *len = 0;
                }
            }
            Self::Heap(heap) => {
                if heap.iter().all(|&c| c == NormalizedCoord::ZERO) {
                    heap.clear();
                }
            }
        }
    }

    fn resize(&mut self, new_len: usize) {
        match self {
            Self::None => {
                if new_len > MAX_INLINE_COORDS {
                    let mut heap = Vec::with_capacity(new_len);
                    heap.resize(new_len, NormalizedCoord::ZERO);
                    *self = Self::Heap(heap);
                } else {
                    *self = Self::Inline([NormalizedCoord::ZERO; MAX_INLINE_COORDS], new_len as u8);
                }
            }
            Self::Inline(_, len) => {
                if new_len > MAX_INLINE_COORDS {
                    let mut heap = Vec::with_capacity(new_len);
                    heap.resize(new_len, NormalizedCoord::ZERO);
                    *self = Self::Heap(heap);
                } else {
                    *len = new_len as u8;
                }
            }
            Self::Heap(heap) => {
                heap.resize(new_len, NormalizedCoord::ZERO);
            }
        }
    }

    fn as_slice(&self) -> &[NormalizedCoord] {
        match self {
            Self::None => &[],
            Self::Inline(coords, len) => &coords[..*len as usize],
            Self::Heap(heap) => heap.as_slice(),
        }
    }

    fn as_mut_slice(&mut self) -> &mut [NormalizedCoord] {
        match self {
            Self::None => &mut [],
            Self::Inline(coords, len) => &mut coords[..*len as usize],
            Self::Heap(heap) => heap.as_mut_slice(),
        }
    }
}

/// Feature variation selections for the layout tables.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct FontFeatureVariations {
    gsub: Option<u32>,
    gpos: Option<u32>,
}

impl FontFeatureVariations {
    /// Returns the selected feature variation index for the GSUB table, if any.
    pub fn gsub(&self) -> Option<u32> {
        self.gsub
    }

    /// Returns the selected feature variation index for the GPOS table, if any.
    pub fn gpos(&self) -> Option<u32> {
        self.gpos
    }
}

/// Lazy atomic storage for feature variation selections.
///
/// We don't want to load the GSUB and GPOS tables unless explicitly requested.
struct FeatureVarsStorage {
    status: AtomicU32,
    gsub: AtomicU32,
    gpos: AtomicU32,
}

impl Default for FeatureVarsStorage {
    fn default() -> Self {
        Self::new()
    }
}

impl FeatureVarsStorage {
    /// We haven't checked yet.
    const UNCHECKED: u32 = 0;
    /// We have a selected feature variation.
    const PRESENT: u32 = 1;
    /// We don't have a selected feature variation.
    const ABSENT: u32 = 2;
    /// Both GSUB and GPOS don't have a selected feature variation.
    const BOTH_ABSENT: u32 = Self::ABSENT | (Self::ABSENT << Self::GPOS_SHIFT);
    /// GPOS status is packed in the high 16 bits. GSUB status is packed in the
    /// low 16 bits.
    const GPOS_SHIFT: u32 = 16;

    fn new() -> Self {
        Self {
            status: AtomicU32::new(Self::UNCHECKED),
            gsub: AtomicU32::new(0),
            gpos: AtomicU32::new(0),
        }
    }

    fn load(&self, font: &Font, coords: &[NormalizedCoord]) -> FontFeatureVariations {
        let mut status = self.status.load(atomic::Ordering::Acquire);
        if status == Self::UNCHECKED {
            let tables = font.tables();
            let feature_var_tables = [
                tables
                    .gsub()
                    .ok()
                    .and_then(|gsub| gsub.feature_variations().transpose().ok().flatten()),
                tables
                    .gpos()
                    .ok()
                    .and_then(|gpos| gpos.feature_variations().transpose().ok().flatten()),
            ];
            for (i, (table, state)) in feature_var_tables
                .iter()
                .zip([&self.gsub, &self.gpos])
                .enumerate()
            {
                let mut table_status = Self::ABSENT;
                if let Some(table) = table {
                    if let Some(index) = feature_variation_index(table, coords) {
                        state.store(index, atomic::Ordering::Release);
                        table_status = Self::PRESENT;
                    }
                }
                status |= table_status << (i * Self::GPOS_SHIFT as usize);
            }
            self.status.store(status, atomic::Ordering::Release);
        }
        if status != Self::BOTH_ABSENT {
            let gsub_status = status & 0xFFFF;
            let gpos_status = (status >> Self::GPOS_SHIFT) & 0xFFFF;
            FontFeatureVariations {
                gsub: (gsub_status == Self::PRESENT)
                    .then(|| self.gsub.load(atomic::Ordering::Acquire)),
                gpos: (gpos_status == Self::PRESENT)
                    .then(|| self.gpos.load(atomic::Ordering::Acquire)),
            }
        } else {
            FontFeatureVariations::default()
        }
    }
}

pub(crate) fn feature_variation_index(
    feature_vars: &FeatureVariations,
    coords: &[NormalizedCoord],
) -> Option<u32> {
    for (index, rec) in feature_vars.feature_variation_records().iter().enumerate() {
        // If the ConditionSet offset is 0, this is treated as the
        // universal condition: all contexts are matched.
        if rec.condition_set_offset().is_null() {
            return Some(index as u32);
        }
        let Some(Ok(condition_set)) = rec.condition_set(feature_vars.offset_data()) else {
            continue;
        };
        // Otherwise, all conditions must be satisfied.
        if condition_set
            .conditions()
            .iter()
            // .. except we ignore errors
            .filter_map(Result::ok)
            .all(|cond| match cond {
                Condition::Format1AxisRange(format1) => {
                    let coord = coords
                        .get(format1.axis_index() as usize)
                        .copied()
                        .unwrap_or_default();
                    coord >= format1.filter_range_min_value()
                        && coord <= format1.filter_range_max_value()
                }
                _ => false,
            })
        {
            return Some(index as u32);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::sync::atomic::Ordering;

    #[test]
    fn named_instances() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let cases = [
            // (named instance index, expected weight value)
            (0, 100.0),
            (1, 300.0),
            (2, 400.0),
            (3, 700.0),
            (4, 800.0),
        ];
        for (index, weight) in cases {
            let named_instance = FontInstance::builder(&font).named_instance(index).build();
            let var_instance = FontInstance::builder(&font)
                .variations([("wght", weight)])
                .build();
            assert_eq!(
                named_instance.normalized_coords(),
                var_instance.normalized_coords(),
                "index={index}"
            );
        }
        // Out of bounds index should give us the default instance.
        let invalid_instance = FontInstance::builder(&font).named_instance(5).build();
        assert!(
            invalid_instance.normalized_coords().is_empty(),
            "out of bounds index should give default instance"
        );
    }

    #[test]
    fn named_instance_with_overrides_override_named_value() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = FontInstance::builder(&font)
            .named_instance_with_overrides(3, [("FILL", 1.0)])
            .build();
        let expected = FontInstance::builder(&font)
            .variations([
                ("FILL", 1.0),
                ("GRAD", 0.0),
                ("opsz", 24.0),
                ("wght", 400.0),
            ])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
    }

    #[test]
    fn named_instance_with_overrides_invalid_index_uses_overrides_only() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = FontInstance::builder(&font)
            .named_instance_with_overrides(999, [("FILL", 1.0), ("ZZZZ", 123.0)])
            .build();
        let expected = FontInstance::builder(&font)
            .variations([("FILL", 1.0), ("ZZZZ", 123.0)])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
        assert_eq!(actual.normalized_coords().len(), 4);
    }

    #[test]
    fn named_instance_with_overrides_overwrites_previous_settings() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let actual = FontInstance::builder(&font)
            .variations([("FILL", 0.0), ("wght", 100.0)])
            .named_instance_with_overrides(5, [("GRAD", -25.0)])
            .build();
        let expected = FontInstance::builder(&font)
            .variations([
                ("FILL", 0.0),
                ("GRAD", -25.0),
                ("opsz", 24.0),
                ("wght", 600.0),
            ])
            .build();
        assert_eq!(actual.normalized_coords(), expected.normalized_coords());
    }

    #[test]
    fn feature_variations() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let cases = [
            // (fill, [GSUB feature variation index, GPOS feature variation index])
            (0.0, [None, None]),
            (0.5, [None, None]),
            (0.98, [None, None]),
            (0.99, [Some(0), None]),
            (1.0, [Some(0), None]),
        ];
        for (fill, [gsub, gpos]) in cases {
            let instance = FontInstance::builder(&font)
                .variations([("FILL", fill)])
                .build();
            let feature_vars = instance.feature_variations();
            let actual = [feature_vars.gsub(), feature_vars.gpos()];
            assert_eq!(actual, [gsub, gpos], "fill={fill}");
        }
    }

    #[test]
    fn feature_variation_cache_marks_both_absent() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = FontInstance::builder(&font)
            .variations([("FILL", 0.5)])
            .build();
        assert_eq!(instance.feature_vars.status.load(Ordering::Acquire), 0);
        assert_eq!(
            instance.feature_variations(),
            FontFeatureVariations::default()
        );
        assert_eq!(
            instance.feature_vars.status.load(Ordering::Acquire),
            FeatureVarsStorage::BOTH_ABSENT
        );
    }

    #[test]
    fn feature_variation_cache_is_thread_safe_and_stable() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = FontInstance::builder(&font)
            .variations([("FILL", 1.0)])
            .build();
        std::thread::scope(|scope| {
            for _ in 0..8 {
                scope.spawn(|| {
                    for _ in 0..64 {
                        let vars = instance.feature_variations();
                        assert_eq!(
                            vars,
                            FontFeatureVariations {
                                gsub: Some(0),
                                gpos: None
                            }
                        );
                    }
                });
            }
        });
        let status = instance.feature_vars.status.load(Ordering::Acquire);
        assert_eq!(status & 0xFFFF, FeatureVarsStorage::PRESENT);
        assert_eq!(
            (status >> FeatureVarsStorage::GPOS_SHIFT) & 0xFFFF,
            FeatureVarsStorage::ABSENT
        );
        assert_eq!(instance.feature_vars.gsub.load(Ordering::Acquire), 0);
    }

    #[test]
    fn variations_last_value_wins_and_unknown_axis_ignored() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let expected = FontInstance::builder(&font)
            .variations([("wght", 700.0)])
            .build();
        let repeated_axis = FontInstance::builder(&font)
            .variations([("wght", 100.0), ("wght", 700.0)])
            .build();
        assert_eq!(
            repeated_axis.normalized_coords(),
            expected.normalized_coords()
        );
        let unknown_axis = FontInstance::builder(&font)
            .variations([("wght", 700.0), ("ZZZZ", 123.0)])
            .build();
        assert_eq!(
            unknown_axis.normalized_coords(),
            expected.normalized_coords()
        );
    }

    #[test]
    fn later_variation_call_overwrites_previous() {
        let font = Font::new(font_test_data::CANTARELL_VF_TRIMMED, 0).unwrap();
        let overwritten = FontInstance::builder(&font)
            .variations([("wght", 700.0)])
            .variations([("wght", 100.0)])
            .build();
        let expected = FontInstance::builder(&font)
            .variations([("wght", 100.0)])
            .build();
        assert_eq!(
            overwritten.normalized_coords(),
            expected.normalized_coords()
        );
    }

    #[test]
    fn normalized_coords_empty_resets_to_default_instance() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let instance = FontInstance::builder(&font).normalized_coords([]).build();
        assert!(instance.normalized_coords().is_empty());
    }

    #[test]
    fn normalized_coords_truncates_and_pads() {
        let font = Font::new(font_test_data::MATERIAL_SYMBOLS_SUBSET, 0).unwrap();
        let axis_count = font.tables().fvar().unwrap().axis_count() as usize;
        let values = [0.25, -0.5, 1.0, 0.75, -0.25].map(NormalizedCoord::from_f32);
        let instance = FontInstance::builder(&font)
            .normalized_coords(values)
            .build();
        let coords = instance.normalized_coords();
        assert_eq!(coords.len(), axis_count);
        let copied = values.len().min(axis_count);
        assert_eq!(&coords[..copied], &values[..copied]);
        assert!(coords[copied..]
            .iter()
            .all(|&coord| coord == NormalizedCoord::ZERO));
    }
}
