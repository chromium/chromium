//! the [VARC (Variable Composite/Component)](https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md) table

use super::variations::PackedDeltas;
pub use super::{
    layout::{Condition, CoverageTable},
    postscript::Index2,
};
use crate::types::F2Dot14;

#[cfg(feature = "libm")]
#[allow(unused_imports)]
use core_maths::*;

include!("../../generated/generated_varc.rs");

/// Let's us call self.something().get(i) instead of get(self.something(), i)
trait Get<'a> {
    fn get(self, nth: usize) -> Result<&'a [u8], ReadError>;
}

impl<'a> Get<'a> for Option<Result<Index2<'a>, ReadError>> {
    fn get(self, nth: usize) -> Result<&'a [u8], ReadError> {
        self.transpose()?
            .ok_or(ReadError::NullOffset)
            .and_then(|index| index.get(nth).map_err(|_| ReadError::OutOfBounds))
    }
}

impl Varc<'_> {
    /// Friendlier accessor than directly using raw data via [Index2]
    pub fn axis_indices(&self, nth: usize) -> Result<PackedDeltas<'_>, ReadError> {
        let raw = self.axis_indices_list().get(nth)?;
        Ok(PackedDeltas::consume_all(raw.into()))
    }

    /// Friendlier accessor than directly using raw data via [Index2]
    ///
    /// nth would typically be obtained by looking up a [GlyphId] in [Self::coverage].
    pub fn glyph(&self, nth: usize) -> Result<VarcGlyph<'_>, ReadError> {
        let raw = Some(self.var_composite_glyphs()).get(nth)?;
        Ok(VarcGlyph {
            table: self,
            data: raw.into(),
        })
    }
}

impl SparseVariationRegion<'_> {
    /// Computes a floating point scalar value for this sparse region and the
    /// specified normalized variation coordinates.
    pub fn compute_scalar_f32(&self, coords: &[F2Dot14]) -> f32 {
        let mut scalar = 1.0f32;
        for axis in self.region_axes() {
            let peak = axis.peak();
            if peak == F2Dot14::ZERO {
                continue;
            }
            let axis_index = axis.axis_index() as usize;
            let coord = coords.get(axis_index).copied().unwrap_or(F2Dot14::ZERO);
            if coord == peak {
                continue;
            }
            if coord == F2Dot14::ZERO {
                return 0.0;
            }
            let start = axis.start();
            let end = axis.end();
            if start > peak || peak > end || (start < F2Dot14::ZERO && end > F2Dot14::ZERO) {
                continue;
            }
            if coord < start || coord > end {
                return 0.0;
            } else if coord < peak {
                // Use raw bits - scale factors cancel in the ratio
                let numerat = coord.to_bits() - start.to_bits();
                if numerat == 0 {
                    return 0.0;
                }
                let denom = peak.to_bits() - start.to_bits();
                scalar *= numerat as f32 / denom as f32;
            } else {
                // Use raw bits - scale factors cancel in the ratio
                let numerat = end.to_bits() - coord.to_bits();
                if numerat == 0 {
                    return 0.0;
                }
                let denom = end.to_bits() - peak.to_bits();
                scalar *= numerat as f32 / denom as f32;
            }
        }
        scalar
    }
}

/// A VARC glyph doesn't have any root level attributes, it's just a list of components
///
/// <https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#variable-composite-description>
pub struct VarcGlyph<'a> {
    table: &'a Varc<'a>,
    data: FontData<'a>,
}

impl<'a> VarcGlyph<'a> {
    /// <https://github.com/fonttools/fonttools/blob/5e6b12d12fa08abafbeb7570f47707fbedf69a45/Lib/fontTools/ttLib/tables/otTables.py#L404-L409>
    pub fn components(&self) -> impl Iterator<Item = Result<VarcComponent<'a>, ReadError>> {
        VarcComponentIter {
            table: self.table,
            cursor: self.data.cursor(),
        }
    }
}

struct VarcComponentIter<'a> {
    table: &'a Varc<'a>,
    cursor: Cursor<'a>,
}

impl<'a> Iterator for VarcComponentIter<'a> {
    type Item = Result<VarcComponent<'a>, ReadError>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cursor.is_empty() {
            return None;
        }
        Some(VarcComponent::parse(self.table, &mut self.cursor))
    }
}

pub struct VarcComponent<'a> {
    flags: VarcFlags,
    gid: GlyphId,
    condition_index: Option<u32>,
    axis_indices_index: Option<u32>,
    axis_values: Option<PackedDeltas<'a>>,
    axis_values_var_index: Option<u32>,
    transform_var_index: Option<u32>,
    transform: DecomposedTransform,
}

impl<'a> VarcComponent<'a> {
    /// Requires access to VARC fields to fully parse.
    ///
    ///  * HarfBuzz [VarComponent::get_path_at](https://github.com/harfbuzz/harfbuzz/blob/0c2f5ecd51d11e32836ee136a1bc765d650a4ec0/src/OT/Var/VARC/VARC.cc#L132)
    fn parse(table: &Varc, cursor: &mut Cursor<'a>) -> Result<Self, ReadError> {
        let raw_flags = cursor.read_u32_var()?;
        let flags = VarcFlags::from_bits_truncate(raw_flags);
        // Ref https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#variable-component-record

        // This is a GlyphID16 if GID_IS_24BIT bit of flags is clear, else GlyphID24.
        let gid = if flags.contains(VarcFlags::GID_IS_24BIT) {
            GlyphId::new(cursor.read::<Uint24>()?.to_u32())
        } else {
            GlyphId::from(cursor.read::<u16>()?)
        };

        let condition_index = if flags.contains(VarcFlags::HAVE_CONDITION) {
            Some(cursor.read_u32_var()?)
        } else {
            None
        };

        let (axis_indices_index, axis_values) = if flags.contains(VarcFlags::HAVE_AXES) {
            // <https://github.com/harfbuzz/harfbuzz/blob/0c2f5ecd51d11e32836ee136a1bc765d650a4ec0/src/OT/Var/VARC/VARC.cc#L195-L206>
            let axis_indices_index = cursor.read_u32_var()?;
            let num_axis_values = table
                .axis_indices(axis_indices_index as usize)?
                .count_or_compute();
            // we need to consume num_axis_values entries in packed delta format
            let deltas = if num_axis_values > 0 {
                let Some(data) = cursor.remaining() else {
                    return Err(ReadError::OutOfBounds);
                };
                let deltas = PackedDeltas::new(data, num_axis_values);
                *cursor = deltas.iter().end(); // jump past the packed deltas
                Some(deltas)
            } else {
                None
            };
            (Some(axis_indices_index), deltas)
        } else {
            (None, None)
        };

        let axis_values_var_index = if flags.contains(VarcFlags::AXIS_VALUES_HAVE_VARIATION) {
            Some(cursor.read_u32_var()?)
        } else {
            None
        };

        let transform_var_index = if flags.contains(VarcFlags::TRANSFORM_HAS_VARIATION) {
            Some(cursor.read_u32_var()?)
        } else {
            None
        };

        let mut transform = DecomposedTransform::default();
        if flags.intersects(VarcFlags::HAVE_TRANSLATE_X | VarcFlags::HAVE_TRANSLATE_Y) {
            if flags.contains(VarcFlags::HAVE_TRANSLATE_X) {
                transform.translate_x = cursor.read::<FWord>()?.to_i16() as f32
            }
            if flags.contains(VarcFlags::HAVE_TRANSLATE_Y) {
                transform.translate_y = cursor.read::<FWord>()?.to_i16() as f32
            }
        }
        if flags.contains(VarcFlags::HAVE_ROTATION) {
            transform.rotation = cursor.read::<F4Dot12>()?.to_f32()
        }
        if flags.intersects(VarcFlags::HAVE_SCALE_X | VarcFlags::HAVE_SCALE_Y) {
            if flags.contains(VarcFlags::HAVE_SCALE_X) {
                transform.scale_x = cursor.read::<F6Dot10>()?.to_f32()
            }
            transform.scale_y = if flags.contains(VarcFlags::HAVE_SCALE_Y) {
                cursor.read::<F6Dot10>()?.to_f32()
            } else {
                transform.scale_x
            };
        }
        if flags.intersects(VarcFlags::HAVE_TCENTER_X | VarcFlags::HAVE_TCENTER_Y) {
            if flags.contains(VarcFlags::HAVE_TCENTER_X) {
                transform.center_x = cursor.read::<FWord>()?.to_i16() as f32
            }
            if flags.contains(VarcFlags::HAVE_TCENTER_Y) {
                transform.center_y = cursor.read::<FWord>()?.to_i16() as f32
            }
        }
        if flags.intersects(VarcFlags::HAVE_SKEW_X | VarcFlags::HAVE_SKEW_Y) {
            if flags.contains(VarcFlags::HAVE_SKEW_X) {
                transform.skew_x = cursor.read::<F4Dot12>()?.to_f32()
            }
            if flags.contains(VarcFlags::HAVE_SKEW_Y) {
                transform.skew_y = cursor.read::<F4Dot12>()?.to_f32()
            }
        }

        // Optional, process and discard one uint32var per each set bit in RESERVED_MASK.
        let reserved = raw_flags & VarcFlags::RESERVED_MASK.bits;
        if reserved != 0 {
            let num_reserved = reserved.count_ones();
            for _ in 0..num_reserved {
                cursor.read_u32_var()?;
            }
        }
        Ok(VarcComponent {
            flags,
            gid,
            condition_index,
            axis_indices_index,
            axis_values,
            axis_values_var_index,
            transform_var_index,
            transform,
        })
    }

    pub fn flags(&self) -> VarcFlags {
        self.flags
    }
    pub fn gid(&self) -> GlyphId {
        self.gid
    }
    pub fn condition_index(&self) -> Option<u32> {
        self.condition_index
    }
    pub fn transform(&self) -> &DecomposedTransform {
        &self.transform
    }
    pub fn axis_indices_index(&self) -> Option<u32> {
        self.axis_indices_index
    }
    pub fn axis_values(&self) -> Option<&PackedDeltas<'a>> {
        self.axis_values.as_ref()
    }
    pub fn axis_values_var_index(&self) -> Option<u32> {
        self.axis_values_var_index
    }
    pub fn transform_var_index(&self) -> Option<u32> {
        self.transform_var_index
    }
}

/// <https://github.com/fonttools/fonttools/blob/5e6b12d12fa08abafbeb7570f47707fbedf69a45/Lib/fontTools/misc/transform.py#L410>
#[derive(Clone, Copy)]
pub struct DecomposedTransform {
    translate_x: f32,
    translate_y: f32,
    rotation: f32, // multiples of Pi, counter-clockwise
    scale_x: f32,
    scale_y: f32,
    skew_x: f32, // multiples of Pi, clockwise
    skew_y: f32, // multiples of Pi, counter-clockwise
    center_x: f32,
    center_y: f32,
}

impl Default for DecomposedTransform {
    fn default() -> Self {
        Self {
            translate_x: 0.0,
            translate_y: 0.0,
            rotation: 0.0,
            scale_x: 1.0,
            scale_y: 1.0,
            skew_x: 0.0,
            skew_y: 0.0,
            center_x: 0.0,
            center_y: 0.0,
        }
    }
}

impl DecomposedTransform {
    pub fn translate_x(&self) -> f32 {
        self.translate_x
    }

    pub fn translate_y(&self) -> f32 {
        self.translate_y
    }

    pub fn rotation(&self) -> f32 {
        self.rotation
    }

    pub fn scale_x(&self) -> f32 {
        self.scale_x
    }

    pub fn scale_y(&self) -> f32 {
        self.scale_y
    }

    pub fn skew_x(&self) -> f32 {
        self.skew_x
    }

    pub fn skew_y(&self) -> f32 {
        self.skew_y
    }

    pub fn center_x(&self) -> f32 {
        self.center_x
    }

    pub fn center_y(&self) -> f32 {
        self.center_y
    }

    pub fn set_translate_x(&mut self, value: f32) {
        self.translate_x = value;
    }

    pub fn set_translate_y(&mut self, value: f32) {
        self.translate_y = value;
    }

    pub fn set_rotation(&mut self, value: f32) {
        self.rotation = value;
    }

    pub fn set_scale_x(&mut self, value: f32) {
        self.scale_x = value;
    }

    pub fn set_scale_y(&mut self, value: f32) {
        self.scale_y = value;
    }

    pub fn set_skew_x(&mut self, value: f32) {
        self.skew_x = value;
    }

    pub fn set_skew_y(&mut self, value: f32) {
        self.skew_y = value;
    }

    pub fn set_center_x(&mut self, value: f32) {
        self.center_x = value;
    }

    pub fn set_center_y(&mut self, value: f32) {
        self.center_y = value;
    }

    /// Convert decomposed form to 2x3 matrix form.
    ///
    /// The first two values are x,y x-basis vector,
    /// the second 2 values are x,y y-basis vector, and the third 2 are translation.
    ///
    /// In augmented matrix
    /// form, if this method returns `[a, b, c, d, e, f]` that is taken as:
    ///
    /// ```text
    /// | a c e |
    /// | b d f |
    /// | 0 0 1 |
    /// ```
    ///
    /// References:
    ///   FontTools Python implementation <https://github.com/fonttools/fonttools/blob/5e6b12d12fa08abafbeb7570f47707fbedf69a45/Lib/fontTools/misc/transform.py#L484-L500>
    /// * Wikipedia [affine transformation](https://en.wikipedia.org/wiki/Affine_transformation)
    pub fn matrix(&self) -> [f32; 6] {
        // Python: t.translate(self.translateX + self.tCenterX, self.translateY + self.tCenterY)
        let mut transform = [
            1.0,
            0.0,
            0.0,
            1.0,
            self.translate_x + self.center_x,
            self.translate_y + self.center_y,
        ];

        // TODO: this produces very small floats for rotations, e.g. 90 degree rotation a basic scale
        // puts 1.2246467991473532e-16 into [0]. Should we special case? Round?

        // Python: t = t.rotate(self.rotation * math.pi)
        if self.rotation != 0.0 {
            let (s, c) = (self.rotation * core::f32::consts::PI).sin_cos();
            transform = transform.transform([c, s, -s, c, 0.0, 0.0]);
        }

        // Python: t = t.scale(self.scaleX, self.scaleY)
        if (self.scale_x, self.scale_y) != (1.0, 1.0) {
            transform = transform.transform([self.scale_x, 0.0, 0.0, self.scale_y, 0.0, 0.0]);
        }

        // Python: t = t.skew(-self.skewX * math.pi, self.skewY * math.pi)
        if (self.skew_x, self.skew_y) != (0.0, 0.0) {
            transform = transform.transform([
                1.0,
                (self.skew_y * core::f32::consts::PI).tan(),
                (-self.skew_x * core::f32::consts::PI).tan(),
                1.0,
                0.0,
                0.0,
            ])
        }

        // Python: t = t.translate(-self.tCenterX, -self.tCenterY)
        if (self.center_x, self.center_y) != (0.0, 0.0) {
            transform = transform.transform([1.0, 0.0, 0.0, 1.0, -self.center_x, -self.center_y]);
        }

        transform
    }
}

trait Transform {
    fn transform(self, other: Self) -> Self;
}

impl Transform for [f32; 6] {
    fn transform(self, other: Self) -> Self {
        // Shamelessly copied from kurbo Affine Mul
        [
            self[0] * other[0] + self[2] * other[1],
            self[1] * other[0] + self[3] * other[1],
            self[0] * other[2] + self[2] * other[3],
            self[1] * other[2] + self[3] * other[3],
            self[0] * other[4] + self[2] * other[5] + self[4],
            self[1] * other[4] + self[3] * other[5] + self[5],
        ]
    }
}

impl<'a> MultiItemVariationData<'a> {
    /// An [Index2] where each item is a [PackedDeltas]
    pub fn delta_sets(&self) -> Result<Index2<'a>, ReadError> {
        Index2::read(self.raw_delta_sets().into())
    }

    /// Read a specific delta set.
    ///
    /// Equivalent to calling [Self::delta_sets], fetching item i, and parsing as [PackedDeltas]
    pub fn delta_set(&self, i: usize) -> Result<PackedDeltas<'a>, ReadError> {
        let index = self.delta_sets()?;
        let raw_deltas = index.get(i).map_err(|_| ReadError::OutOfBounds)?;
        Ok(PackedDeltas::consume_all(raw_deltas.into()))
    }
}

#[cfg(test)]
mod tests {
    use types::GlyphId16;

    use crate::types::F2Dot14;
    use crate::FontData;
    use crate::{FontRef, ReadError, TableProvider};

    use super::{Condition, DecomposedTransform, Varc};

    impl Varc<'_> {
        fn conditions(&self) -> impl Iterator<Item = Condition<'_>> {
            self.condition_list()
                .expect("A condition list is present")
                .expect("We could read the condition list")
                .conditions()
                .iter()
                .enumerate()
                .map(|(i, c)| c.unwrap_or_else(|e| panic!("condition {i} {e}")))
        }

        fn axis_indices_count(&self) -> Result<usize, ReadError> {
            let Some(axis_indices_list) = self.axis_indices_list() else {
                return Ok(0);
            };
            let axis_indices_list = axis_indices_list?;
            Ok(axis_indices_list.count() as usize)
        }
    }

    fn round6(v: f32) -> f32 {
        (v * 1_000_000.0).round() / 1_000_000.0
    }

    fn coord(value: f32) -> F2Dot14 {
        F2Dot14::from_f32(value)
    }

    fn assert_close(actual: f32, expected: f32) {
        let diff = (actual - expected).abs();
        assert!(
            diff <= 1e-6,
            "expected {expected}, got {actual}, diff {diff}"
        );
    }

    fn sfnt_table_range(data: &[u8], tag: [u8; 4]) -> (usize, usize) {
        let font = FontRef::new(data).unwrap();
        if let Some(rec) = font
            .table_directory()
            .table_records()
            .iter()
            .find(|rec| rec.tag() == tag)
        {
            return (rec.offset() as usize, rec.length() as usize);
        }
        panic!(
            "missing table {:?}",
            core::str::from_utf8(&tag).unwrap_or("????")
        );
    }

    fn write_be_u32(dst: &mut [u8], value: u32) {
        dst.copy_from_slice(&value.to_be_bytes());
    }

    fn read_be_u32(src: &[u8]) -> u32 {
        u32::from_be_bytes([src[0], src[1], src[2], src[3]])
    }

    fn encode_u32_var(value: u32) -> Vec<u8> {
        if value < 0x80 {
            vec![value as u8]
        } else if value < 0x4000 {
            vec![0x80 | ((value >> 8) as u8), value as u8]
        } else if value < 0x20_0000 {
            vec![
                0xC0 | ((value >> 16) as u8),
                (value >> 8) as u8,
                value as u8,
            ]
        } else if value < 0x1000_0000 {
            vec![
                0xE0 | ((value >> 24) as u8),
                (value >> 16) as u8,
                (value >> 8) as u8,
                value as u8,
            ]
        } else {
            vec![
                0xF0,
                (value >> 24) as u8,
                (value >> 16) as u8,
                (value >> 8) as u8,
                value as u8,
            ]
        }
    }

    trait Round {
        fn round_for_test(self) -> Self;
    }

    impl Round for [f32; 6] {
        fn round_for_test(self) -> Self {
            [
                round6(self[0]),
                round6(self[1]),
                round6(self[2]),
                round6(self[3]),
                round6(self[4]),
                round6(self[5]),
            ]
        }
    }

    #[test]
    fn read_cjk_0x6868() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        table.coverage().unwrap(); // should have coverage
    }

    #[test]
    fn identify_all_conditional_types() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();

        // We should have all 5 condition types in order
        assert_eq!(
            (1..=5).collect::<Vec<_>>(),
            table.conditions().map(|c| c.format()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn read_condition_format1_axis_range() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        let Some(Condition::Format1AxisRange(condition)) =
            table.conditions().find(|c| c.format() == 1)
        else {
            panic!("No such item");
        };

        assert_eq!(
            (0, 0.5, 1.0),
            (
                condition.axis_index(),
                condition.filter_range_min_value().to_f32(),
                condition.filter_range_max_value().to_f32(),
            )
        );
    }

    #[test]
    fn read_condition_format2_variable_value() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        let Some(Condition::Format2VariableValue(condition)) =
            table.conditions().find(|c| c.format() == 2)
        else {
            panic!("No such item");
        };

        assert_eq!((1, 2), (condition.default_value(), condition.var_index(),));
    }

    #[test]
    fn read_condition_format3_and() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        let Some(Condition::Format3And(condition)) = table.conditions().find(|c| c.format() == 3)
        else {
            panic!("No such item");
        };

        // Should reference a format 1 and a format 2
        assert_eq!(
            vec![1, 2],
            condition
                .conditions()
                .iter()
                .map(|c| c.unwrap().format())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn read_condition_format4_or() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        let Some(Condition::Format4Or(condition)) = table.conditions().find(|c| c.format() == 4)
        else {
            panic!("No such item");
        };

        // Should reference a format 1 and a format 2
        assert_eq!(
            vec![1, 2],
            condition
                .conditions()
                .iter()
                .map(|c| c.unwrap().format())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn read_condition_format5_negate() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        let Some(Condition::Format5Negate(condition)) =
            table.conditions().find(|c| c.format() == 5)
        else {
            panic!("No such item");
        };

        // Should reference a format 1
        assert_eq!(1, condition.condition().unwrap().format(),);
    }

    #[test]
    fn read_axis_indices_list() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices_count().unwrap(), 2);
        assert_eq!(
            vec![2, 3, 4, 5, 6],
            table.axis_indices(1).unwrap().iter().collect::<Vec<_>>()
        );
    }

    #[test]
    fn compute_sparse_region_scalar_handles_boundaries_and_products() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let varc = font.varc().unwrap();
        let store = varc.multi_var_store().unwrap().unwrap();
        let regions = store.region_list().unwrap();
        let region_list = regions.regions();

        let axis0_region = region_list.get(0).unwrap();
        assert_close(axis0_region.compute_scalar_f32(&[coord(1.0)]), 1.0);
        assert_close(axis0_region.compute_scalar_f32(&[coord(0.5)]), 0.5);
        assert_close(axis0_region.compute_scalar_f32(&[F2Dot14::ZERO]), 0.0);
        assert_close(axis0_region.compute_scalar_f32(&[]), 0.0);
        assert_close(axis0_region.compute_scalar_f32(&[coord(-0.25)]), 0.0);

        let axis0_axis1_region = region_list.get(2).unwrap();
        assert_close(
            axis0_axis1_region.compute_scalar_f32(&[coord(0.5), coord(0.25)]),
            0.125,
        );
    }

    #[test]
    fn axis_indices_offset_out_of_bounds_errors() {
        let mut bytes = font_test_data::varc::CJK_6868.to_vec();
        let (varc_offset, varc_len) = sfnt_table_range(&bytes, *b"VARC");
        // axis_indices_list_offset is the 5th u32 in the VARC header.
        let axis_indices_offset = varc_offset + 16;
        write_be_u32(
            &mut bytes[axis_indices_offset..axis_indices_offset + 4],
            (varc_len as u32).saturating_add(8),
        );

        let font = FontRef::new(&bytes).unwrap();
        let table = font.varc().unwrap();
        assert!(matches!(table.axis_indices(0), Err(ReadError::OutOfBounds)));
    }

    #[test]
    fn var_composite_glyphs_offset_out_of_bounds_errors() {
        let mut bytes = font_test_data::varc::CJK_6868.to_vec();
        let (varc_offset, varc_len) = sfnt_table_range(&bytes, *b"VARC");
        // var_composite_glyphs_offset is the 6th u32 in the VARC header.
        let glyphs_offset = varc_offset + 20;
        write_be_u32(
            &mut bytes[glyphs_offset..glyphs_offset + 4],
            (varc_len as u32).saturating_add(8),
        );

        let font = FontRef::new(&bytes).unwrap();
        let table = font.varc().unwrap();
        assert!(matches!(table.glyph(0), Err(ReadError::OutOfBounds)));
    }

    #[test]
    fn parse_component_with_missing_translate_data_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        // flags=HAVE_TRANSLATE_X, gid=1, missing FWORD payload.
        let data = FontData::new(&[0x10, 0x00, 0x01]);
        let mut cursor = data.cursor();
        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_with_invalid_axis_indices_index_errors() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        // flags=HAVE_AXES, gid=1, axis_indices_index=127 (out of range).
        let data = FontData::new(&[0x02, 0x00, 0x01, 0x7F]);
        let mut cursor = data.cursor();
        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_reserved_fields_are_consumed() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        // flags = HAVE_TRANSLATE_X plus two reserved bits.
        let flags = 0x0001_8010_u32;
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(flags));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&[0x00, 0x07]); // translate_x = 7
        bytes.extend_from_slice(&encode_u32_var(1)); // reserved payload 1
        bytes.extend_from_slice(&encode_u32_var(2)); // reserved payload 2
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.transform().translate_x(), 7.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_reserved_fields_truncation_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        // flags = HAVE_TRANSLATE_X plus two reserved bits, but only one reserved payload follows.
        let flags = 0x0001_8010_u32;
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(flags));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&[0x00, 0x07]); // translate_x
        bytes.extend_from_slice(&encode_u32_var(1)); // only one reserved payload
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_gid_is_24bit_path() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::GID_IS_24BIT.bits()));
        bytes.extend_from_slice(&[0x12, 0x34, 0x56]);
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.gid().to_u32(), 0x12_34_56);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_gid_is_24bit_truncation_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::GID_IS_24BIT.bits()));
        bytes.extend_from_slice(&[0x12, 0x34]); // truncated uint24
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_with_axes_zero_count_does_not_consume_axis_values() {
        let mut bytes = font_test_data::varc::CJK_6868.to_vec();
        let (varc_offset, _) = sfnt_table_range(&bytes, *b"VARC");
        let axis_indices_rel = read_be_u32(&bytes[varc_offset + 16..varc_offset + 20]) as usize;
        let axis_indices_abs = varc_offset + axis_indices_rel;
        // Overwrite axis_indices_list with Index2 { count = 1, off_size = 1, offsets = [1, 1] }.
        // That single item is empty, so count_or_compute() must be zero.
        bytes[axis_indices_abs..axis_indices_abs + 7]
            .copy_from_slice(&[0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01]);
        let font = FontRef::new(&bytes).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(0).unwrap().count_or_compute(), 0);

        // flags = HAVE_AXES | HAVE_TRANSLATE_X
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(0x12));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(0)); // axis_indices_index, count is zero
        bytes.extend_from_slice(&[0x00, 0x05]); // translate_x
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.transform().translate_x(), 5.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_with_axes_nonzero_count_consumes_axis_values() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(1).unwrap().count_or_compute(), 5);

        // flags = HAVE_AXES | HAVE_TRANSLATE_X
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(0x12));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(1)); // axis_indices_index, count is five
        bytes.push(0x04); // one I8 run, count = 5
        bytes.extend_from_slice(&[1, 2, 3, 4, 5]); // packed axis values
        bytes.extend_from_slice(&[0x00, 0x09]); // translate_x
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.transform().translate_x(), 9.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_scale_x_only_applies_to_scale_y() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_SCALE_X.bits()));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&[0x08, 0x00]); // scale_x = 2.0 in F6Dot10
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.transform().scale_x(), 2.0);
        assert_eq!(component.transform().scale_y(), 2.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_scale_x_and_scale_y_are_independent() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let flags = super::VarcFlags::HAVE_SCALE_X.bits() | super::VarcFlags::HAVE_SCALE_Y.bits();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(flags));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&[0x08, 0x00]); // scale_x = 2.0
        bytes.extend_from_slice(&[0x0C, 0x00]); // scale_y = 3.0
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.transform().scale_x(), 2.0);
        assert_eq!(component.transform().scale_y(), 3.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_multibyte_condition_and_var_indices() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let flags = super::VarcFlags::HAVE_CONDITION.bits()
            | super::VarcFlags::AXIS_VALUES_HAVE_VARIATION.bits()
            | super::VarcFlags::TRANSFORM_HAS_VARIATION.bits()
            | super::VarcFlags::HAVE_TRANSLATE_X.bits();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(flags));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(0x2345)); // condition_index
        bytes.extend_from_slice(&encode_u32_var(0x0123)); // axis_values_var_index
        bytes.extend_from_slice(&encode_u32_var(0x0222)); // transform_var_index
        bytes.extend_from_slice(&[0x00, 0x07]); // translate_x = 7
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.condition_index(), Some(0x2345));
        assert_eq!(component.axis_values_var_index(), Some(0x0123));
        assert_eq!(component.transform_var_index(), Some(0x0222));
        assert_eq!(component.transform().translate_x(), 7.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_truncated_multibyte_condition_index_errors() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_CONDITION.bits()));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.push(0x81); // truncated uint32var (needs one more byte)
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_axes_with_var_index_and_zero_axis_count() {
        let mut bytes = font_test_data::varc::CJK_6868.to_vec();
        let (varc_offset, _) = sfnt_table_range(&bytes, *b"VARC");
        let axis_indices_rel = read_be_u32(&bytes[varc_offset + 16..varc_offset + 20]) as usize;
        let axis_indices_abs = varc_offset + axis_indices_rel;
        // One empty axis-indices entry at index 0.
        bytes[axis_indices_abs..axis_indices_abs + 7]
            .copy_from_slice(&[0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01]);
        let font = FontRef::new(&bytes).unwrap();
        let table = font.varc().unwrap();

        let flags = super::VarcFlags::HAVE_AXES.bits()
            | super::VarcFlags::AXIS_VALUES_HAVE_VARIATION.bits()
            | super::VarcFlags::HAVE_TRANSLATE_X.bits();
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(flags));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(0)); // axis_indices_index => zero-length axis list
        bytes.extend_from_slice(&encode_u32_var(0x0123)); // axis_values_var_index
        bytes.extend_from_slice(&[0x00, 0x06]); // translate_x = 6
        bytes.push(0xAA); // sentinel
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        assert_eq!(component.axis_indices_index(), Some(0));
        assert!(component.axis_values().is_none());
        assert_eq!(component.axis_values_var_index(), Some(0x0123));
        assert_eq!(component.transform().translate_x(), 6.0);
        assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
    }

    #[test]
    fn parse_component_axes_truncated_i16_payload_detected_on_fetch() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(1).unwrap().count_or_compute(), 5);

        // flags = HAVE_AXES.
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_AXES.bits()));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(1)); // axis_indices_index => expects 5 values
        bytes.push(0x44); // I16 run, count = 5
        bytes.extend_from_slice(&[0x00, 0x01, 0x00, 0x02, 0x00, 0x03]); // only 3 i16 values
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        let mut out = [0.0; 5];
        assert!(matches!(
            component
                .axis_values()
                .unwrap()
                .fetcher()
                .add_to_f32_scaled(&mut out, 1.0),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_axes_truncated_i32_payload_detected_on_fetch() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(1).unwrap().count_or_compute(), 5);

        // flags = HAVE_AXES.
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_AXES.bits()));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(1)); // axis_indices_index => expects 5 values
        bytes.push(0xC4); // I32 run, count = 5
        bytes.extend_from_slice(&[
            0x00, 0x00, 0x00, 0x01, // only 1 i32 value
            0x00, 0x00, 0x00, 0x02,
        ]);
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
        let mut out = [0.0; 5];
        assert!(matches!(
            component
                .axis_values()
                .unwrap()
                .fetcher()
                .add_to_f32_scaled(&mut out, 1.0),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn parse_component_axes_payload_overshoot_causes_following_field_error() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(1).unwrap().count_or_compute(), 5);

        // flags = HAVE_AXES | HAVE_TRANSLATE_X.
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&encode_u32_var(
            super::VarcFlags::HAVE_AXES.bits() | super::VarcFlags::HAVE_TRANSLATE_X.bits(),
        ));
        bytes.extend_from_slice(&[0x00, 0x01]); // gid
        bytes.extend_from_slice(&encode_u32_var(1)); // axis_indices_index => expects 5 values
        bytes.push(0x44); // I16 run, count = 5 (needs 10 bytes)
        bytes.extend_from_slice(&[0x00, 0x01, 0x00, 0x02]); // too short
        bytes.extend_from_slice(&[0x00, 0x09]); // intended translate_x (should be unreachable)
        let data = FontData::new(&bytes);
        let mut cursor = data.cursor();

        assert!(matches!(
            super::VarcComponent::parse(&table, &mut cursor),
            Err(ReadError::OutOfBounds)
        ));
    }

    #[test]
    fn generated_condition_varints_roundtrip() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let mut state = 0x1234_5678_u32;
        for _ in 0..256 {
            // xorshift32
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            let value = state;

            let mut bytes = Vec::new();
            bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_CONDITION.bits()));
            bytes.extend_from_slice(&[0x00, 0x01]); // gid
            bytes.extend_from_slice(&encode_u32_var(value));
            bytes.push(0xAA); // sentinel
            let data = FontData::new(&bytes);
            let mut cursor = data.cursor();

            let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
            assert_eq!(component.condition_index(), Some(value));
            assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
        }
    }

    #[test]
    fn generated_axis_values_single_run_roundtrip() {
        let font = FontRef::new(font_test_data::varc::CONDITIONALS).unwrap();
        let table = font.varc().unwrap();
        assert_eq!(table.axis_indices(1).unwrap().count_or_compute(), 5);

        let mut state = 0xA5A5_5A5A_u32;
        for _ in 0..128 {
            // xorshift32
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            let run_kind = state & 3;

            let mut payload = Vec::new();
            let mut expected = [0f32; 5];
            match run_kind {
                0 => {
                    // I8, count = 5
                    payload.push(0x04);
                    for out in &mut expected {
                        state = state.wrapping_mul(1664525).wrapping_add(1013904223);
                        let v = ((state >> 24) as i8) % 64;
                        payload.push(v as u8);
                        *out = v as f32;
                    }
                }
                1 => {
                    // I16, count = 5
                    payload.push(0x44);
                    for out in &mut expected {
                        state = state.wrapping_mul(1664525).wrapping_add(1013904223);
                        let v = ((state >> 16) as i16) % 2048;
                        payload.extend_from_slice(&v.to_be_bytes());
                        *out = v as f32;
                    }
                }
                2 => {
                    // I32, count = 5
                    payload.push(0xC4);
                    for out in &mut expected {
                        state = state.wrapping_mul(1664525).wrapping_add(1013904223);
                        let v = (state as i32) % 1_000_000;
                        payload.extend_from_slice(&v.to_be_bytes());
                        *out = v as f32;
                    }
                }
                _ => {
                    // Zero, count = 5
                    payload.push(0x84);
                }
            }

            let mut bytes = Vec::new();
            bytes.extend_from_slice(&encode_u32_var(
                super::VarcFlags::HAVE_AXES.bits() | super::VarcFlags::HAVE_TRANSLATE_X.bits(),
            ));
            bytes.extend_from_slice(&[0x00, 0x01]); // gid
            bytes.extend_from_slice(&encode_u32_var(1)); // axis_indices_index => 5 values
            bytes.extend_from_slice(&payload);
            bytes.extend_from_slice(&[0x00, 0x07]); // translate_x
            bytes.push(0xAA); // sentinel

            let data = FontData::new(&bytes);
            let mut cursor = data.cursor();
            let component = super::VarcComponent::parse(&table, &mut cursor).unwrap();
            let mut out = [0.0f32; 5];
            component
                .axis_values()
                .unwrap()
                .fetcher()
                .add_to_f32_scaled(&mut out, 1.0)
                .unwrap();
            assert_eq!(out, expected);
            assert_eq!(component.transform().translate_x(), 7.0);
            assert_eq!(cursor.read::<u8>().unwrap(), 0xAA);
        }
    }

    #[test]
    fn parse_component_truncated_condition_varint_boundaries_error() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let values = [0x80_u32, 0x4000_u32, 0x20_0000_u32, 0x1000_0000_u32];

        for value in values {
            let encoded = encode_u32_var(value);
            assert!(encoded.len() > 1);
            let truncated = &encoded[..encoded.len() - 1];

            let mut bytes = Vec::new();
            bytes.extend_from_slice(&encode_u32_var(super::VarcFlags::HAVE_CONDITION.bits()));
            bytes.extend_from_slice(&[0x00, 0x01]); // gid
            bytes.extend_from_slice(truncated);
            let data = FontData::new(&bytes);
            let mut cursor = data.cursor();

            assert!(matches!(
                super::VarcComponent::parse(&table, &mut cursor),
                Err(ReadError::OutOfBounds)
            ));
        }
    }

    #[test]
    fn read_glyph_6868() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let gid = font.cmap().unwrap().map_codepoint(0x6868_u32).unwrap();
        let table = font.varc().unwrap();
        let idx = table.coverage().unwrap().get(gid).unwrap();

        let glyph = table.glyph(idx as usize).unwrap();
        assert_eq!(
            vec![GlyphId16::new(2), GlyphId16::new(5), GlyphId16::new(7)],
            glyph
                .components()
                .map(|c| c.unwrap().gid)
                .collect::<Vec<_>>()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_scale_to_matrix() {
        let scale_x = 2.0;
        let scale_y = 3.0;
        assert_eq!(
            [scale_x, 0.0, 0.0, scale_y, 0.0, 0.0],
            DecomposedTransform {
                scale_x,
                scale_y,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_rotate_to_matrix() {
        assert_eq!(
            [0.0, 1.0, -1.0, 0.0, 0.0, 0.0],
            DecomposedTransform {
                // Rotation is in multiples of Pi (90 degrees = 0.5 * Pi).
                rotation: 0.5,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_skew_to_matrix() {
        // Skew is in multiples of Pi.
        let skew_x: f32 = 1.0 / 6.0; // 30 degrees
        let skew_y: f32 = -1.0 / 3.0; // -60 degrees
        assert_eq!(
            [
                1.0,
                round6((skew_y * core::f32::consts::PI).tan()),
                round6((-skew_x * core::f32::consts::PI).tan()),
                1.0,
                0.0,
                0.0
            ],
            DecomposedTransform {
                skew_x,
                skew_y,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_scale_rotate_to_matrix() {
        let scale_x = 2.0;
        let scale_y = 3.0;
        assert_eq!(
            [0.0, scale_x, -scale_y, 0.0, 0.0, 0.0],
            DecomposedTransform {
                scale_x,
                scale_y,
                // 90 degrees = 0.5 * Pi.
                rotation: 0.5,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_scale_rotate_translate_to_matrix() {
        assert_eq!(
            [0.0, 2.0, -1.0, 0.0, 10.0, 20.0],
            DecomposedTransform {
                scale_x: 2.0,
                // 90 degrees = 0.5 * Pi.
                rotation: 0.5,
                translate_x: 10.0,
                translate_y: 20.0,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_scale_skew_translate_to_matrix() {
        assert_eq!(
            [-0.866026, 5.5, -2.5, 2.020726, 10.0, 20.0],
            DecomposedTransform {
                scale_x: 2.0,
                scale_y: 3.0,
                // Angles are in multiples of Pi.
                rotation: 1.0 / 6.0, // 30 degrees
                skew_x: 1.0 / 6.0,   // 30 degrees
                skew_y: 1.0 / 3.0,   // 60 degrees
                translate_x: 10.0,
                translate_y: 20.0,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    // Expected created using the Python DecomposedTransform
    #[test]
    fn decomposed_rotate_around_to_matrix() {
        assert_eq!(
            [1.732051, 1.0, -0.5, 0.866025, 10.267949, 19.267949],
            DecomposedTransform {
                scale_x: 2.0,
                // 30 degrees = 1/6 * Pi.
                rotation: 1.0 / 6.0,
                translate_x: 10.0,
                translate_y: 20.0,
                center_x: 1.0,
                center_y: 2.0,
                ..Default::default()
            }
            .matrix()
            .round_for_test()
        );
    }

    #[test]
    fn read_multivar_store_region_list() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let varstore = table.multi_var_store().unwrap().unwrap();
        let regions = varstore.region_list().unwrap().regions();

        let sparse_regions = regions
            .iter()
            .map(|r| {
                r.unwrap()
                    .region_axes()
                    .iter()
                    .map(|a| {
                        (
                            a.axis_index(),
                            a.start().to_f32(),
                            a.peak().to_f32(),
                            a.end().to_f32(),
                        )
                    })
                    .collect::<Vec<_>>()
            })
            .collect::<Vec<_>>();

        // Check a sampling of the regions
        assert_eq!(
            vec![
                vec![(0, 0.0, 1.0, 1.0),],
                vec![(0, 0.0, 1.0, 1.0), (1, 0.0, 1.0, 1.0),],
                vec![(6, -1.0, -1.0, 0.0),],
            ],
            [0, 2, 38]
                .into_iter()
                .map(|i| sparse_regions[i].clone())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn read_multivar_store_delta_sets() {
        let font = FontRef::new(font_test_data::varc::CJK_6868).unwrap();
        let table = font.varc().unwrap();
        let varstore = table.multi_var_store().unwrap().unwrap();
        assert_eq!(
            vec![(3, 6), (33, 6), (10, 5), (25, 8),],
            varstore
                .variation_data()
                .iter()
                .map(|d| d.unwrap())
                .map(|d| (d.region_index_count(), d.delta_sets().unwrap().count()))
                .collect::<Vec<_>>()
        );
        assert_eq!(
            vec![-1, 33, 0, 0, 0, 0],
            varstore
                .variation_data()
                .get(0)
                .unwrap()
                .delta_set(5)
                .unwrap()
                .iter()
                .collect::<Vec<_>>()
        )
    }
}
