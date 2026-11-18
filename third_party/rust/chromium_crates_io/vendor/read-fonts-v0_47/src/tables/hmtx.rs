//! The [hmtx (Horizontal Metrics)](https://docs.microsoft.com/en-us/typography/opentype/spec/hmtx) table

include!("../../generated/generated_hmtx.rs");

use crate::{table_provider::prefer_extended, TableProvider};

macro_rules! impl_hmtx_lookup {
    ($table:ident) => {
        impl $table<'_> {
            /// Returns the advance width for the given glyph identifier.
            pub fn advance(&self, glyph_id: GlyphId) -> Option<u16> {
                advance(self.h_metrics(), glyph_id)
            }

            /// Returns the left side bearing for the given glyph identifier.
            pub fn side_bearing(&self, glyph_id: GlyphId) -> Option<i16> {
                side_bearing(self.h_metrics(), self.left_side_bearings(), glyph_id)
            }
        }
    };
}

impl_hmtx_lookup!(Hmtx);
impl_hmtx_lookup!(HmtxExtended);

pub(crate) fn advance(metrics: &[LongMetric], glyph_id: GlyphId) -> Option<u16> {
    metrics
        .get(glyph_id.to_u32() as usize)
        .or_else(|| metrics.last())
        .map(|metric| metric.advance())
}

pub(crate) fn side_bearing(
    metrics: &[LongMetric],
    side_bearings: &[BigEndian<i16>],
    glyph_id: GlyphId,
) -> Option<i16> {
    let ix = glyph_id.to_u32() as usize;
    metrics
        .get(ix)
        .map(|metric| metric.side_bearing())
        .or_else(|| {
            side_bearings
                .get(ix.saturating_sub(metrics.len()))
                .map(|sb| sb.get())
        })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};
    use font_test_data::{be_buffer, bebuffer::BeBuffer};

    /// Test case where "long metric" array is short
    #[test]
    fn trimmed_advances() {
        let font = FontRef::new(font_test_data::CBDT).unwrap();
        let hmtx = font.hmtx().unwrap();
        assert!(
            !hmtx.left_side_bearings().is_empty(),
            "if this fails then the test is no longer accurate"
        );
        let expected_lsbs = [100, 0, 100, 0];
        for (i, lsb) in expected_lsbs.into_iter().enumerate() {
            let gid = GlyphId::new(i as _);
            // All glyphs have 800 advance width
            assert_eq!(hmtx.advance(gid), Some(800));
            assert_eq!(hmtx.side_bearing(gid), Some(lsb));
        }
    }

    #[test]
    fn missing_left_side_bearings() {
        let hmtx_data = be_buffer! {
            500u16, 50u16, // advance width + lsb, glyph 0
            600u16, 60u16 // advance width + lsb, glyph 1
        };

        let hmtx = Hmtx::read(hmtx_data.data().into(), 2).unwrap();

        assert_eq!(hmtx.advance(GlyphId::new(0)), Some(500));
        assert_eq!(hmtx.side_bearing(GlyphId::new(0)), Some(50));

        assert_eq!(hmtx.advance(GlyphId::new(1)), Some(600));
        assert_eq!(hmtx.side_bearing(GlyphId::new(1)), Some(60));

        assert_eq!(hmtx.advance(GlyphId::new(2)), Some(600));
        assert_eq!(hmtx.side_bearing(GlyphId::new(2)), None);

        assert_eq!(hmtx.advance(GlyphId::new(3)), Some(600));
        assert_eq!(hmtx.side_bearing(GlyphId::new(3)), None);
    }

    #[test]
    fn metrics() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let hmtx = font.hmtx().unwrap();
        let expected = [(908, 100), (1336, 29), (1336, 29), (633, 57)];
        for (i, (advance, lsb)) in expected.into_iter().enumerate() {
            let gid = GlyphId::new(i as _);
            assert_eq!(hmtx.advance(gid), Some(advance));
            assert_eq!(hmtx.side_bearing(gid), Some(lsb));
        }
    }
}

/// Unvaried per-glyph records from HMTX/hmtx or VMTX/vmtx.
///
/// Both tables hold the same records, so one type reads either, and which
/// direction it describes is fixed when it is read.
///
/// Obtain these through [`TableProvider::h_glyph_metric_records`] or
/// [`TableProvider::v_glyph_metric_records`]. Uppercase tables are selected
/// independently of the outlines, with the original tables used if they are
/// absent.
/// Nothing here depends on a location, so every location shares one parse.
#[derive(Clone, Default)]
pub struct GlyphMetricRecords<'a> {
    pub(crate) metrics: &'a [LongMetric],
    /// Bare side bearings after the last long metric.
    pub(crate) bearings: &'a [BigEndian<i16>],
}

/// The records of one table, whichever the font would select.
macro_rules! glyph_metric_records_from {
    ($($table:ty => $metrics:ident, $bearings:ident;)*) => {
        $(
            impl<'a> From<$table> for GlyphMetricRecords<'a> {
                fn from(table: $table) -> Self {
                    Self {
                        metrics: table.$metrics(),
                        bearings: table.$bearings(),
                    }
                }
            }
        )*
    };
}

glyph_metric_records_from! {
    Hmtx<'a> => h_metrics, left_side_bearings;
    HmtxExtended<'a> => h_metrics, left_side_bearings;
    super::vmtx::Vmtx<'a> => v_metrics, top_side_bearings;
    super::vmtx::VmtxExtended<'a> => v_metrics, top_side_bearings;
}

impl<'a> GlyphMetricRecords<'a> {
    pub(crate) fn read_horizontal(
        tables: &(impl TableProvider<'a> + ?Sized),
    ) -> Result<Self, ReadError> {
        prefer_extended(
            tables.hmtx_extended().map(Self::from),
            Tag::new(b"HMTX"),
            || tables.hmtx().map(Self::from),
        )
    }

    pub(crate) fn read_vertical(
        tables: &(impl TableProvider<'a> + ?Sized),
    ) -> Result<Self, ReadError> {
        prefer_extended(
            tables.vmtx_extended().map(Self::from),
            Tag::new(b"VMTX"),
            || tables.vmtx().map(Self::from),
        )
    }

    /// Returns the paired advance and side-bearing records.
    pub fn long_metrics(&self) -> &'a [LongMetric] {
        self.metrics
    }

    /// Returns the side bearings after the last long metric record.
    pub fn side_bearings(&self) -> &'a [BigEndian<i16>] {
        self.bearings
    }

    /// Returns the unvaried advance, repeating the last record when needed.
    /// This does not check the font's glyph count.
    pub fn advance(&self, glyph: GlyphId) -> Option<u16> {
        advance(self.metrics, glyph)
    }

    /// Returns the unvaried leading side bearing for a glyph.
    pub fn side_bearing(&self, glyph: GlyphId) -> Option<i16> {
        side_bearing(self.metrics, self.bearings, glyph)
    }
}
