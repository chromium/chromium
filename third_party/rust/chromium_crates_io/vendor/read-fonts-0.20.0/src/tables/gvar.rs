//! The [gvar (Glyph Variations)](https://learn.microsoft.com/en-us/typography/opentype/spec/gvar)
//! table

include!("../../generated/generated_gvar.rs");

use super::{
    glyf::PointCoord,
    variations::{
        PackedPointNumbers, Tuple, TupleDelta, TupleVariationCount, TupleVariationData,
        TupleVariationHeader,
    },
};

/// Variation data specialized for the glyph variations table.
pub type GlyphVariationData<'a> = TupleVariationData<'a, GlyphDelta>;

#[derive(Clone, Copy, Debug)]
pub struct U16Or32(u32);

impl ReadArgs for U16Or32 {
    type Args = GvarFlags;
}

impl ComputeSize for U16Or32 {
    fn compute_size(args: &GvarFlags) -> Result<usize, ReadError> {
        Ok(if args.contains(GvarFlags::LONG_OFFSETS) {
            4
        } else {
            2
        })
    }
}

impl FontReadWithArgs<'_> for U16Or32 {
    fn read_with_args(data: FontData<'_>, args: &Self::Args) -> Result<Self, ReadError> {
        if args.contains(GvarFlags::LONG_OFFSETS) {
            data.read_at::<u32>(0).map(Self)
        } else {
            data.read_at::<u16>(0).map(|v| Self(v as u32 * 2))
        }
    }
}

impl U16Or32 {
    #[inline]
    pub fn get(self) -> u32 {
        self.0
    }
}

impl<'a> GlyphVariationDataHeader<'a> {
    fn raw_tuple_header_data(&self) -> FontData<'a> {
        let range = self.shape.tuple_variation_headers_byte_range();
        self.data.split_off(range.start).unwrap()
    }
}

impl<'a> Gvar<'a> {
    fn data_for_gid(&self, gid: GlyphId) -> Result<FontData<'a>, ReadError> {
        let start_idx = gid.to_u32() as usize;
        let end_idx = start_idx + 1;
        let data_start = self.glyph_variation_data_array_offset();
        let start = data_start + self.glyph_variation_data_offsets().get(start_idx)?.get();
        let end = data_start + self.glyph_variation_data_offsets().get(end_idx)?.get();

        self.data
            .slice(start as usize..end as usize)
            .ok_or(ReadError::OutOfBounds)
    }

    /// Get the variation data for a specific glyph.
    pub fn glyph_variation_data(&self, gid: GlyphId) -> Result<GlyphVariationData<'a>, ReadError> {
        let shared_tuples = self.shared_tuples()?;
        let axis_count = self.axis_count();
        let data = self.data_for_gid(gid)?;
        GlyphVariationData::new(data, axis_count, shared_tuples)
    }
}

impl<'a> GlyphVariationData<'a> {
    pub(crate) fn new(
        data: FontData<'a>,
        axis_count: u16,
        shared_tuples: SharedTuples<'a>,
    ) -> Result<Self, ReadError> {
        let header = GlyphVariationDataHeader::read(data)?;

        let header_data = header.raw_tuple_header_data();
        let count = header.tuple_variation_count();
        let data = header.serialized_data()?;

        // if there are shared point numbers, get them now
        let (shared_point_numbers, serialized_data) =
            if header.tuple_variation_count().shared_point_numbers() {
                let (packed, data) = PackedPointNumbers::split_off_front(data);
                (Some(packed), data)
            } else {
                (None, data)
            };

        Ok(GlyphVariationData {
            tuple_count: count,
            axis_count,
            shared_tuples: Some(shared_tuples),
            shared_point_numbers,
            header_data,
            serialized_data,
            _marker: std::marker::PhantomData,
        })
    }
}

/// Delta information for a single point or component in a glyph.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct GlyphDelta {
    /// The point or component index.
    pub position: u16,
    /// The x delta.
    pub x_delta: i32,
    /// The y delta.
    pub y_delta: i32,
}

impl GlyphDelta {
    /// Applies a tuple scalar to this delta.
    pub fn apply_scalar<D: PointCoord>(self, scalar: Fixed) -> Point<D> {
        let scalar = D::from_fixed(scalar);
        Point::new(self.x_delta, self.y_delta).map(D::from_i32) * scalar
    }
}

impl TupleDelta for GlyphDelta {
    fn is_point() -> bool {
        true
    }

    fn new(position: u16, x: i32, y: i32) -> Self {
        Self {
            position,
            x_delta: x,
            y_delta: y,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};

    // Shared tuples in the 'gvar' table of the Skia font, as printed
    // in Apple's TrueType specification.
    // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6gvar.html
    static SKIA_GVAR_SHARED_TUPLES_DATA: FontData = FontData::new(&[
        0x40, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xC0,
        0x00, 0xC0, 0x00, 0xC0, 0x00, 0x40, 0x00, 0xC0, 0x00, 0x40, 0x00, 0x40, 0x00, 0xC0, 0x00,
        0x40, 0x00,
    ]);

    static SKIA_GVAR_I_DATA: FontData = FontData::new(&[
        0x00, 0x08, 0x00, 0x24, 0x00, 0x33, 0x20, 0x00, 0x00, 0x15, 0x20, 0x01, 0x00, 0x1B, 0x20,
        0x02, 0x00, 0x24, 0x20, 0x03, 0x00, 0x15, 0x20, 0x04, 0x00, 0x26, 0x20, 0x07, 0x00, 0x0D,
        0x20, 0x06, 0x00, 0x1A, 0x20, 0x05, 0x00, 0x40, 0x01, 0x01, 0x01, 0x81, 0x80, 0x43, 0xFF,
        0x7E, 0xFF, 0x7E, 0xFF, 0x7E, 0xFF, 0x7E, 0x00, 0x81, 0x45, 0x01, 0x01, 0x01, 0x03, 0x01,
        0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x02, 0x80, 0x40, 0x00, 0x82, 0x81, 0x81, 0x04, 0x3A,
        0x5A, 0x3E, 0x43, 0x20, 0x81, 0x04, 0x0E, 0x40, 0x15, 0x45, 0x7C, 0x83, 0x00, 0x0D, 0x9E,
        0xF3, 0xF2, 0xF0, 0xF0, 0xF0, 0xF0, 0xF3, 0x9E, 0xA0, 0xA1, 0xA1, 0xA1, 0x9F, 0x80, 0x00,
        0x91, 0x81, 0x91, 0x00, 0x0D, 0x0A, 0x0A, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0A, 0x0A, 0x0A, 0x0B, 0x80, 0x00, 0x15, 0x81, 0x81, 0x00, 0xC4, 0x89, 0x00, 0xC4, 0x83,
        0x00, 0x0D, 0x80, 0x99, 0x98, 0x96, 0x96, 0x96, 0x96, 0x99, 0x80, 0x82, 0x83, 0x83, 0x83,
        0x81, 0x80, 0x40, 0xFF, 0x18, 0x81, 0x81, 0x04, 0xE6, 0xF9, 0x10, 0x21, 0x02, 0x81, 0x04,
        0xE8, 0xE5, 0xEB, 0x4D, 0xDA, 0x83, 0x00, 0x0D, 0xCE, 0xD3, 0xD4, 0xD3, 0xD3, 0xD3, 0xD5,
        0xD2, 0xCE, 0xCC, 0xCD, 0xCD, 0xCD, 0xCD, 0x80, 0x00, 0xA1, 0x81, 0x91, 0x00, 0x0D, 0x07,
        0x03, 0x04, 0x02, 0x02, 0x02, 0x03, 0x03, 0x07, 0x07, 0x08, 0x08, 0x08, 0x07, 0x80, 0x00,
        0x09, 0x81, 0x81, 0x00, 0x28, 0x40, 0x00, 0xA4, 0x02, 0x24, 0x24, 0x66, 0x81, 0x04, 0x08,
        0xFA, 0xFA, 0xFA, 0x28, 0x83, 0x00, 0x82, 0x02, 0xFF, 0xFF, 0xFF, 0x83, 0x02, 0x01, 0x01,
        0x01, 0x84, 0x91, 0x00, 0x80, 0x06, 0x07, 0x08, 0x08, 0x08, 0x08, 0x0A, 0x07, 0x80, 0x03,
        0xFE, 0xFF, 0xFF, 0xFF, 0x81, 0x00, 0x08, 0x81, 0x82, 0x02, 0xEE, 0xEE, 0xEE, 0x8B, 0x6D,
        0x00,
    ]);

    #[test]
    fn test_shared_tuples() {
        #[allow(overflowing_literals)]
        const MINUS_ONE: F2Dot14 = F2Dot14::from_bits(0xC000);
        assert_eq!(MINUS_ONE, F2Dot14::from_f32(-1.0));

        static EXPECTED: &[(F2Dot14, F2Dot14)] = &[
            (F2Dot14::ONE, F2Dot14::ZERO),
            (MINUS_ONE, F2Dot14::ZERO),
            (F2Dot14::ZERO, F2Dot14::ONE),
            (F2Dot14::ZERO, MINUS_ONE),
            (MINUS_ONE, MINUS_ONE),
            (F2Dot14::ONE, MINUS_ONE),
            (F2Dot14::ONE, F2Dot14::ONE),
            (MINUS_ONE, F2Dot14::ONE),
        ];

        const N_AXES: u16 = 2;

        let tuples =
            SharedTuples::read(SKIA_GVAR_SHARED_TUPLES_DATA, EXPECTED.len() as u16, N_AXES)
                .unwrap();
        let tuple_vec: Vec<_> = tuples
            .tuples()
            .iter()
            .map(|tup| {
                let values = tup.unwrap().values();
                assert_eq!(values.len(), N_AXES as usize);
                (values[0].get(), values[1].get())
            })
            .collect();

        assert_eq!(tuple_vec, EXPECTED);
    }

    // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6gvar.html
    #[test]
    fn smoke_test() {
        let header = GlyphVariationDataHeader::read(SKIA_GVAR_I_DATA).unwrap();
        assert_eq!(header.serialized_data_offset(), 36);
        assert_eq!(header.tuple_variation_count().count(), 8);
        let shared_tuples = SharedTuples::read(SKIA_GVAR_SHARED_TUPLES_DATA, 8, 2).unwrap();

        let vardata = GlyphVariationData::new(SKIA_GVAR_I_DATA, 2, shared_tuples).unwrap();
        assert_eq!(vardata.tuple_count(), 8);
        let deltas = vardata
            .tuples()
            .next()
            .unwrap()
            .deltas()
            .collect::<Vec<_>>();
        assert_eq!(deltas.len(), 18);
        static EXPECTED: &[(i32, i32)] = &[
            (257, 0),
            (-127, 0),
            (-128, 58),
            (-130, 90),
            (-130, 62),
            (-130, 67),
            (-130, 32),
            (-127, 0),
            (257, 0),
            (259, 14),
            (260, 64),
            (260, 21),
            (260, 69),
            (258, 124),
            (0, 0),
            (130, 0),
            (0, 0),
            (0, 0),
        ];
        let expected = EXPECTED
            .iter()
            .copied()
            .enumerate()
            .map(|(pos, (x_delta, y_delta))| GlyphDelta {
                position: pos as _,
                x_delta,
                y_delta,
            })
            .collect::<Vec<_>>();

        for (a, b) in deltas.iter().zip(expected.iter()) {
            assert_eq!(a, b);
        }
    }

    #[test]
    fn vazirmatn_var_a() {
        let gvar = FontRef::new(font_test_data::VAZIRMATN_VAR)
            .unwrap()
            .gvar()
            .unwrap();
        let a_glyph_var = gvar.glyph_variation_data(GlyphId::new(1)).unwrap();
        assert_eq!(a_glyph_var.axis_count, 1);
        let mut tuples = a_glyph_var.tuples();
        let tup1 = tuples.next().unwrap();
        assert_eq!(tup1.peak().values(), &[F2Dot14::from_f32(-1.0)]);
        assert_eq!(tup1.deltas().count(), 18);
        let x_vals = &[
            -90, -134, 4, -6, -81, 18, -25, -33, -109, -121, -111, -111, -22, -22, 0, -113, 0, 0,
        ];
        let y_vals = &[83, 0, 0, 0, 0, 0, 83, 0, 0, 0, -50, 54, 54, -50, 0, 0, 0, 0];
        assert_eq!(tup1.deltas().map(|d| d.x_delta).collect::<Vec<_>>(), x_vals);
        assert_eq!(tup1.deltas().map(|d| d.y_delta).collect::<Vec<_>>(), y_vals);
        let tup2 = tuples.next().unwrap();
        assert_eq!(tup2.peak().values(), &[F2Dot14::from_f32(1.0)]);
        let x_vals = &[
            20, 147, -33, -53, 59, -90, 37, -6, 109, 90, -79, -79, -8, -8, 0, 59, 0, 0,
        ];
        let y_vals = &[
            -177, 0, 0, 0, 0, 0, -177, 0, 0, 0, 4, -109, -109, 4, 0, 0, 0, 0,
        ];

        assert_eq!(tup2.deltas().map(|d| d.x_delta).collect::<Vec<_>>(), x_vals);
        assert_eq!(tup2.deltas().map(|d| d.y_delta).collect::<Vec<_>>(), y_vals);
        assert!(tuples.next().is_none());
    }

    #[test]
    fn vazirmatn_var_agrave() {
        let gvar = FontRef::new(font_test_data::VAZIRMATN_VAR)
            .unwrap()
            .gvar()
            .unwrap();
        let agrave_glyph_var = gvar.glyph_variation_data(GlyphId::new(2)).unwrap();
        let mut tuples = agrave_glyph_var.tuples();
        let tup1 = tuples.next().unwrap();
        assert_eq!(
            tup1.deltas()
                .map(|d| (d.position, d.x_delta, d.y_delta))
                .collect::<Vec<_>>(),
            &[(1, -51, 8), (3, -113, 0)]
        );
        let tup2 = tuples.next().unwrap();
        assert_eq!(
            tup2.deltas()
                .map(|d| (d.position, d.x_delta, d.y_delta))
                .collect::<Vec<_>>(),
            &[(1, -54, -1), (3, 59, 0)]
        );
    }

    #[test]
    fn vazirmatn_var_grave() {
        let gvar = FontRef::new(font_test_data::VAZIRMATN_VAR)
            .unwrap()
            .gvar()
            .unwrap();
        let grave_glyph_var = gvar.glyph_variation_data(GlyphId::new(3)).unwrap();
        let mut tuples = grave_glyph_var.tuples();
        let tup1 = tuples.next().unwrap();
        let tup2 = tuples.next().unwrap();
        assert!(tuples.next().is_none());
        assert_eq!(tup1.deltas().count(), 8);
        assert_eq!(
            tup2.deltas().map(|d| d.y_delta).collect::<Vec<_>>(),
            &[0, -20, -20, 0, 0, 0, 0, 0]
        );
    }
}
