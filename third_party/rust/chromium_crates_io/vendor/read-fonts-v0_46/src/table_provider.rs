//! a trait for things that can serve font tables

use types::{BigEndian, Tag};

use crate::{tables, FontData, FontRead, ReadError};

/// A table that has an associated tag.
///
/// This is true of top-level tables, but not their various subtables.
pub trait TopLevelTable {
    /// The table's tag.
    const TAG: Tag;
}

pub(crate) fn prefer_extended<T>(
    extended: Result<T, ReadError>,
    tag: Tag,
    original: impl FnOnce() -> Result<T, ReadError>,
) -> Result<T, ReadError> {
    match extended {
        Err(ReadError::TableIsMissing(missing)) if missing == tag => original(),
        result => result,
    }
}

/// An interface for accessing tables from a font (or font-like object)
pub trait TableProvider<'a> {
    fn data_for_tag(&self, tag: Tag) -> Option<FontData<'a>>;

    fn expect_data_for_tag(&self, tag: Tag) -> Result<FontData<'a>, ReadError> {
        self.data_for_tag(tag).ok_or(ReadError::TableIsMissing(tag))
    }

    fn expect_table<T: TopLevelTable + FontRead<'a, Args = ()>>(&self) -> Result<T, ReadError> {
        self.expect_data_for_tag(T::TAG).and_then(FontRead::read)
    }

    fn head(&self) -> Result<tables::head::Head<'a>, ReadError> {
        self.expect_table()
    }

    fn name(&self) -> Result<tables::name::Name<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the `hhea` table only. A font may carry its metrics header in
    /// HHEA instead; [`selected_hhea`](Self::selected_hhea) chooses between them.
    fn hhea(&self) -> Result<tables::hhea::Hhea<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the `vhea` table only. A font may carry its vertical metrics
    /// header in VHEA instead; [`selected_vhea`](Self::selected_vhea) chooses
    /// between them.
    fn vhea(&self) -> Result<tables::vhea::Vhea<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the uppercase HHEA table.
    fn hhea_extended(&self) -> Result<tables::hhea::HheaExtended<'a>, ReadError> {
        self.expect_table()
    }

    /// Selects HHEA before hhea, independently of the outline tables.
    fn selected_hhea(&self) -> Result<tables::hhea::SelectedHhea<'a>, ReadError> {
        use tables::hhea::SelectedHhea;
        prefer_extended(
            self.hhea_extended().map(SelectedHhea::Extended),
            Tag::new(b"HHEA"),
            || self.hhea().map(SelectedHhea::Original),
        )
    }

    /// Reads the uppercase VHEA table.
    fn vhea_extended(&self) -> Result<tables::vhea::VheaExtended<'a>, ReadError> {
        self.expect_table()
    }

    /// Selects VHEA before vhea, independently of the outline tables.
    fn selected_vhea(&self) -> Result<tables::vhea::SelectedVhea<'a>, ReadError> {
        use tables::vhea::SelectedVhea;
        prefer_extended(
            self.vhea_extended().map(SelectedVhea::Extended),
            Tag::new(b"VHEA"),
            || self.vhea().map(SelectedVhea::Original),
        )
    }

    fn hmtx(&self) -> Result<tables::hmtx::Hmtx<'a>, ReadError> {
        //FIXME: should we make the user pass these in?
        let number_of_h_metrics = self.hhea().map(|hhea| hhea.number_of_h_metrics())?;
        let data = self.expect_data_for_tag(tables::hmtx::Hmtx::TAG)?;
        tables::hmtx::Hmtx::read(data, number_of_h_metrics)
    }

    /// Reads HMTX using the long-metric count from HHEA.
    fn hmtx_extended(&self) -> Result<tables::hmtx::HmtxExtended<'a>, ReadError> {
        let data = self.expect_data_for_tag(tables::hmtx::HmtxExtended::TAG)?;
        let count = self.hhea_extended()?.number_of_h_metrics();
        tables::hmtx::HmtxExtended::read(data, count)
    }

    /// Returns horizontal metrics records, selecting HMTX before hmtx
    /// independently of the outline tables. No allocation is required.
    fn h_glyph_metric_records(
        &self,
    ) -> Result<crate::tables::hmtx::GlyphMetricRecords<'a>, ReadError> {
        crate::tables::hmtx::GlyphMetricRecords::read_horizontal(self)
    }

    fn hdmx(&self) -> Result<tables::hdmx::Hdmx<'a>, ReadError> {
        let num_glyphs = self.selected_maxp()?.num_glyphs();
        let data = self.expect_data_for_tag(tables::hdmx::Hdmx::TAG)?;
        tables::hdmx::Hdmx::read(data, num_glyphs)
    }

    fn vmtx(&self) -> Result<tables::vmtx::Vmtx<'a>, ReadError> {
        //FIXME: should we make the user pass these in?
        let number_of_v_metrics = self.vhea().map(|vhea| vhea.number_of_long_ver_metrics())?;
        let data = self.expect_data_for_tag(tables::vmtx::Vmtx::TAG)?;
        tables::vmtx::Vmtx::read(data, number_of_v_metrics)
    }

    /// Reads VMTX using the long-metric count from VHEA.
    fn vmtx_extended(&self) -> Result<tables::vmtx::VmtxExtended<'a>, ReadError> {
        let data = self.expect_data_for_tag(tables::vmtx::VmtxExtended::TAG)?;
        let count = self.vhea_extended()?.number_of_long_ver_metrics();
        tables::vmtx::VmtxExtended::read(data, count)
    }

    /// Returns vertical metrics records, selecting VMTX before vmtx
    /// independently of the outline tables. No allocation is required.
    fn v_glyph_metric_records(
        &self,
    ) -> Result<crate::tables::hmtx::GlyphMetricRecords<'a>, ReadError> {
        crate::tables::hmtx::GlyphMetricRecords::read_vertical(self)
    }

    fn vorg(&self) -> Result<tables::vorg::Vorg<'a>, ReadError> {
        self.expect_table()
    }

    fn fvar(&self) -> Result<tables::fvar::Fvar<'a>, ReadError> {
        self.expect_table()
    }

    fn avar(&self) -> Result<tables::avar::Avar<'a>, ReadError> {
        self.expect_table()
    }

    fn hvar(&self) -> Result<tables::hvar::Hvar<'a>, ReadError> {
        self.expect_table()
    }

    fn vvar(&self) -> Result<tables::vvar::Vvar<'a>, ReadError> {
        self.expect_table()
    }

    fn mvar(&self) -> Result<tables::mvar::Mvar<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the `maxp` table only. A font with more than 65535 glyphs
    /// carries its glyph count in MAXP; [`selected_maxp`](Self::selected_maxp)
    /// chooses between them.
    fn maxp(&self) -> Result<tables::maxp::Maxp<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the uppercase MAXP table with a 24-bit glyph count.
    fn maxp_extended(&self) -> Result<tables::maxp::MaxpExtended<'a>, ReadError> {
        self.expect_table()
    }

    /// Selects MAXP before maxp without narrowing its glyph count.
    fn selected_maxp(&self) -> Result<tables::maxp::SelectedMaxp<'a>, ReadError> {
        use tables::maxp::SelectedMaxp;
        prefer_extended(
            self.maxp_extended().map(SelectedMaxp::Extended),
            Tag::new(b"MAXP"),
            || self.maxp().map(SelectedMaxp::Original),
        )
    }

    fn os2(&self) -> Result<tables::os2::Os2<'a>, ReadError> {
        self.expect_table()
    }

    fn post(&self) -> Result<tables::post::Post<'a>, ReadError> {
        self.expect_table()
    }

    fn gasp(&self) -> Result<tables::gasp::Gasp<'a>, ReadError> {
        self.expect_table()
    }

    /// is_long can be optionally provided, if known, otherwise we look it up in head.
    fn loca(&self, is_long: impl Into<Option<bool>>) -> Result<tables::loca::Loca<'a>, ReadError> {
        let is_long = match is_long.into() {
            Some(val) => val,
            None => self.head()?.index_to_loc_format() == 1,
        };
        let data = self.expect_data_for_tag(tables::loca::Loca::TAG)?;
        tables::loca::Loca::read(data, is_long)
    }

    /// Reads the `glyf` table only. A font may carry its outlines in GLYF and
    /// LOCA instead; [`selected_glyf_loca`](Self::selected_glyf_loca) chooses
    /// between the pairs.
    fn glyf(&self) -> Result<tables::glyf::Glyf<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads GLYF using the shared glyph data representation.
    fn glyf_extended(&self) -> Result<tables::glyf::Glyf<'a>, ReadError> {
        tables::glyf::Glyf::read(self.expect_data_for_tag(Tag::new(b"GLYF"))?)
    }

    /// Reads LOCA; its format is shared with loca and specified in head.
    fn loca_extended(
        &self,
        is_long: impl Into<Option<bool>>,
    ) -> Result<tables::loca::Loca<'a>, ReadError> {
        let data = self.expect_data_for_tag(Tag::new(b"LOCA"))?;
        let is_long = match is_long.into() {
            Some(val) => val,
            None => self.head()?.index_to_loc_format() == 1,
        };
        tables::loca::Loca::read(data, is_long)
    }

    /// Selects GLYF/LOCA before glyf/loca, independently of metrics.
    /// A present GLYF requires LOCA; it never uses the original loca offsets.
    fn selected_glyf_loca(
        &self,
        is_long: impl Into<Option<bool>>,
    ) -> Result<(tables::glyf::Glyf<'a>, tables::loca::Loca<'a>), ReadError> {
        let is_long = is_long.into();
        prefer_extended(
            self.glyf_extended()
                .and_then(|glyf| self.loca_extended(is_long).map(|loca| (glyf, loca))),
            Tag::new(b"GLYF"),
            || {
                self.glyf()
                    .and_then(|glyf| self.loca(is_long).map(|loca| (glyf, loca)))
            },
        )
    }

    /// Reads the `gvar` table only. A font may carry its glyph variations in
    /// GVAR instead; [`selected_gvar`](Self::selected_gvar) chooses between
    /// them.
    fn gvar(&self) -> Result<tables::gvar::Gvar<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the `GVAR` table only (ISO/IEC 14496-22:2026).
    fn gvar_extended(&self) -> Result<tables::gvar::GvarExtended<'a>, ReadError> {
        self.expect_table()
    }

    /// Selects GVAR before the original gvar table.
    fn selected_gvar(&self) -> Result<tables::gvar::SelectedGvar<'a>, ReadError> {
        prefer_extended(
            self.gvar_extended()
                .map(tables::gvar::SelectedGvar::Extended),
            Tag::new(b"GVAR"),
            || self.gvar().map(tables::gvar::SelectedGvar::Original),
        )
    }

    /// Returns the array of entries for the control value table which is used
    /// for TrueType hinting.
    fn cvt(&self) -> Result<&'a [BigEndian<i16>], ReadError> {
        let table_data = self.expect_data_for_tag(Tag::new(b"cvt "))?;
        table_data.read_array(0..table_data.len())
    }

    /// Returns the font program, which contains the TrueType instructions
    /// run once per font.
    fn fpgm(&self) -> Result<&'a [u8], ReadError> {
        self.expect_data_for_tag(Tag::new(b"fpgm"))
            .map(|data| data.as_bytes())
    }

    /// Returns the control value program, which contains the TrueType
    /// instructions run once per size.
    fn prep(&self) -> Result<&'a [u8], ReadError> {
        self.expect_data_for_tag(Tag::new(b"prep"))
            .map(|data| data.as_bytes())
    }

    fn cvar(&self) -> Result<tables::cvar::Cvar<'a>, ReadError> {
        self.expect_table()
    }

    fn cff(&self) -> Result<tables::cff::Cff<'a>, ReadError> {
        self.expect_table()
    }

    fn cff2(&self) -> Result<tables::cff2::Cff2<'a>, ReadError> {
        self.expect_table()
    }

    fn cmap(&self) -> Result<tables::cmap::Cmap<'a>, ReadError> {
        self.expect_table()
    }

    /// Reads the `DMAP` table (ISO/IEC 14496-22:2026).
    fn dmap(&self) -> Result<tables::cmap::Dmap<'a>, ReadError> {
        self.expect_table()
    }

    fn gdef(&self) -> Result<tables::gdef::Gdef<'a>, ReadError> {
        self.expect_table()
    }

    fn gpos(&self) -> Result<tables::gpos::Gpos<'a>, ReadError> {
        self.expect_table()
    }

    fn gsub(&self) -> Result<tables::gsub::Gsub<'a>, ReadError> {
        self.expect_table()
    }

    fn feat(&self) -> Result<tables::feat::Feat<'a>, ReadError> {
        self.expect_table()
    }

    fn ltag(&self) -> Result<tables::ltag::Ltag<'a>, ReadError> {
        self.expect_table()
    }

    fn ankr(&self) -> Result<tables::ankr::Ankr<'a>, ReadError> {
        self.expect_table()
    }

    fn trak(&self) -> Result<tables::trak::Trak<'a>, ReadError> {
        self.expect_table()
    }

    fn morx(&self) -> Result<tables::morx::Morx<'a>, ReadError> {
        self.expect_table()
    }

    fn mort(&self) -> Result<tables::mort::Mort<'a>, ReadError> {
        self.expect_table()
    }

    fn kerx(&self) -> Result<tables::kerx::Kerx<'a>, ReadError> {
        self.expect_table()
    }

    fn kern(&self) -> Result<tables::kern::Kern<'a>, ReadError> {
        self.expect_table()
    }

    fn colr(&self) -> Result<tables::colr::Colr<'a>, ReadError> {
        self.expect_table()
    }

    fn cpal(&self) -> Result<tables::cpal::Cpal<'a>, ReadError> {
        self.expect_table()
    }

    fn cblc(&self) -> Result<tables::cblc::Cblc<'a>, ReadError> {
        self.expect_table()
    }

    fn cbdt(&self) -> Result<tables::cbdt::Cbdt<'a>, ReadError> {
        self.expect_table()
    }

    fn eblc(&self) -> Result<tables::eblc::Eblc<'a>, ReadError> {
        self.expect_table()
    }

    fn ebdt(&self) -> Result<tables::ebdt::Ebdt<'a>, ReadError> {
        self.expect_table()
    }

    fn sbix(&self) -> Result<tables::sbix::Sbix<'a>, ReadError> {
        // should we make the user pass this in?
        let num_glyphs = self.selected_maxp()?.num_glyphs();
        let data = self.expect_data_for_tag(tables::sbix::Sbix::TAG)?;
        tables::sbix::Sbix::read(data, num_glyphs)
    }

    fn stat(&self) -> Result<tables::stat::Stat<'a>, ReadError> {
        self.expect_table()
    }

    fn svg(&self) -> Result<tables::svg::Svg<'a>, ReadError> {
        self.expect_table()
    }

    fn varc(&self) -> Result<tables::varc::Varc<'a>, ReadError> {
        self.expect_table()
    }

    #[cfg(feature = "ift")]
    fn ift(&self) -> Result<tables::ift::IftPatchMap<'a>, ReadError> {
        self.expect_data_for_tag(tables::ift::IFT_TAG)
            .and_then(FontRead::read)
    }

    #[cfg(feature = "ift")]
    fn iftx(&self) -> Result<tables::ift::IftPatchMap<'a>, ReadError> {
        self.expect_data_for_tag(tables::ift::IFTX_TAG)
            .and_then(FontRead::read)
    }

    fn meta(&self) -> Result<tables::meta::Meta<'a>, ReadError> {
        self.expect_table()
    }

    fn base(&self) -> Result<tables::base::Base<'a>, ReadError> {
        self.expect_table()
    }

    fn dsig(&self) -> Result<tables::dsig::Dsig<'a>, ReadError> {
        self.expect_table()
    }

    fn math(&self) -> Result<tables::math::Math<'a>, ReadError> {
        self.expect_table()
    }
}

#[cfg(test)]
mod tests {

    use super::*;

    #[test]
    fn extended_metrics_are_selected_independently_of_outlines() {
        use crate::{types::GlyphId, FontRef};
        use font_test_data::extended::metrics_font;
        for outline in [None, Some(*b"glyf"), Some(*b"GLYF")] {
            for (extended, legacy) in [(true, false), (true, true), (false, true)] {
                let data = metrics_font(extended, legacy, outline);
                let font = FontRef::new(&data).unwrap();
                let expected_count = if extended { 70002 } else { 3 };
                let long_count = if extended { 70000 } else { 2 };
                assert_eq!(font.selected_maxp().unwrap().num_glyphs(), expected_count);
                assert_eq!(
                    font.selected_hhea().unwrap().number_of_h_metrics(),
                    long_count
                );
                assert_eq!(
                    font.selected_vhea().unwrap().number_of_long_ver_metrics(),
                    long_count
                );
                let horizontal = font.h_glyph_metric_records().unwrap();
                let vertical = font.v_glyph_metric_records().unwrap();
                assert_eq!(horizontal.long_metrics().len() as u32, long_count);
                assert_eq!(vertical.long_metrics().len() as u32, long_count);
                assert_eq!(
                    horizontal.side_bearings().len(),
                    if extended { 2 } else { 1 }
                );
                assert_eq!(
                    horizontal.advance(GlyphId::new(0)),
                    Some(if extended { 1000 } else { 500 })
                );
                assert_eq!(
                    vertical.advance(GlyphId::new(0)),
                    Some(if extended { 1200 } else { 600 })
                );
                if extended {
                    for gid in [65535, 65536, 69999] {
                        assert_eq!(
                            horizontal.advance(GlyphId::new(gid)),
                            Some(1000 + (gid % 100) as u16)
                        );
                        assert_eq!(
                            horizontal.side_bearing(GlyphId::new(gid)),
                            Some(-((gid % 300) as i16))
                        );
                    }
                    for gid in [70000, 70001] {
                        assert_eq!(horizontal.advance(GlyphId::new(gid)), Some(1099));
                        assert_eq!(vertical.advance(GlyphId::new(gid)), Some(1299));
                        assert_eq!(
                            horizontal.side_bearing(GlyphId::new(gid)),
                            Some(-(10 + (gid - 70000) as i16))
                        );
                    }
                    assert_eq!(horizontal.side_bearing(GlyphId::new(70002)), None);
                }
            }
        }
    }

    #[test]
    fn extended_glyph_count_does_not_require_extended_metrics() {
        use crate::{types::GlyphId, FontRef};
        use font_test_data::extended::{font, maxp, metric_data, metric_header};
        let profile = maxp(70002, true);
        let header = metric_header(2, false, false);
        let records = metric_data(2, 1, 500);
        let data = font(&[
            (*b"MAXP", &profile),
            (*b"hhea", &header),
            (*b"hmtx", &records),
        ]);
        let font = FontRef::new(&data).unwrap();
        assert_eq!(font.selected_maxp().unwrap().num_glyphs(), 70002);
        let metrics = font.h_glyph_metric_records().unwrap();
        assert_eq!(metrics.long_metrics().len(), 2);
        assert_eq!(metrics.advance(GlyphId::new(2)), Some(501));
        assert_eq!(metrics.side_bearing(GlyphId::new(2)), Some(-10));
    }

    #[test]
    fn malformed_extended_tables_do_not_fall_back_to_legacy() {
        use crate::{types::GlyphId, FontRef};
        use font_test_data::extended::{font, maxp, metric_data, metric_header};
        let profile = maxp(3, false);
        let header = metric_header(2, false, false);
        let records = metric_data(2, 1, 500);
        let data = font(&[
            (*b"MAXP", &[0, 0]),
            (*b"maxp", &profile),
            (*b"HHEA", &[0, 0]),
            (*b"hhea", &header),
            (*b"hmtx", &records),
        ]);
        let font_ref = FontRef::new(&data).unwrap();
        assert!(font_ref.selected_maxp().is_err());
        assert!(font_ref.selected_hhea().is_err());
        // Metrics selection is independent of the malformed uppercase header
        // when there is no HMTX table to use it.
        assert_eq!(
            font_ref
                .h_glyph_metric_records()
                .unwrap()
                .advance(GlyphId::new(0)),
            Some(500)
        );
        let data = font(&[
            (*b"HMTX", &records),
            (*b"hhea", &header),
            (*b"hmtx", &records),
        ]);
        let font_ref = FontRef::new(&data).unwrap();
        assert!(font_ref.h_glyph_metric_records().is_err());
    }

    #[test]
    fn extended_metrics_with_truncated_or_extreme_counts() {
        use crate::{types::GlyphId, FontRef};
        use font_test_data::extended::{font, metric_data, metric_header};
        let records = metric_data(2, 1, 500);
        for count in [0, 3, u32::MAX] {
            let horizontal = metric_header(count, true, false);
            let vertical = metric_header(count, true, true);
            let data = font(&[
                (*b"HHEA", &horizontal),
                (*b"HMTX", &records),
                (*b"VHEA", &vertical),
                (*b"VMTX", &records),
            ]);
            let font = FontRef::new(&data).unwrap();
            // Like the legacy readers, truncated long-metric arrays read as
            // empty rather than exposing a partial record array.
            for metrics in [
                font.h_glyph_metric_records().unwrap(),
                font.v_glyph_metric_records().unwrap(),
            ] {
                assert!(metrics.long_metrics().is_empty());
                assert_eq!(metrics.advance(GlyphId::new(0)), None);
                assert_eq!(metrics.advance(GlyphId::new(u32::MAX)), None);
                if count != 0 {
                    assert!(metrics.side_bearings().is_empty());
                    assert_eq!(metrics.side_bearing(GlyphId::new(0)), None);
                }
            }
        }
    }

    #[test]
    fn extended_outline_tables_are_selected_as_a_pair() {
        use crate::{tables::glyf::Glyph, types::GlyphId, FontRef};
        use font_test_data::extended::outlines_font;
        for long_loca in [false, true] {
            for legacy in [false, true] {
                for extended_metrics in [false, true] {
                    let data = outlines_font(true, legacy, extended_metrics, long_loca);
                    let font = FontRef::new(&data).unwrap();
                    let (glyf, loca) = font.selected_glyf_loca(None).unwrap();
                    assert_eq!(loca.len(), 65537);
                    let Glyph::Composite(composite) = loca
                        .get(GlyphId::new(1), &glyf)
                        .unwrap()
                        .into_glyph()
                        .unwrap()
                    else {
                        panic!("expected composite")
                    };
                    assert_eq!(
                        composite.components().next().unwrap().glyph,
                        GlyphId::new(65536)
                    );
                    assert_eq!(
                        composite.component_glyphs_and_flags().next().unwrap().0,
                        GlyphId::new(65536)
                    );
                    assert_eq!(composite.count_and_instructions(), (1, None));
                    assert!(loca
                        .get(GlyphId::new(65536), &glyf)
                        .unwrap()
                        .glyph()
                        .is_some());
                    assert!(loca.get(GlyphId::new(65537), &glyf).is_none());
                }
            }
        }
    }

    #[test]
    fn extended_outline_pair_does_not_use_legacy_locations() {
        use crate::FontRef;
        use font_test_data::extended::{font, outlines_font};
        let data = outlines_font(true, true, true, true);
        let source = FontRef::new(&data).unwrap();
        let tables: Vec<_> = source
            .table_directory()
            .table_records()
            .iter()
            .filter(|record| record.tag() != Tag::new(b"LOCA"))
            .map(|record| {
                (
                    record.tag().to_be_bytes(),
                    source.table_data(record.tag()).unwrap().as_bytes(),
                )
            })
            .collect();
        let data = font(&tables);
        let source = FontRef::new(&data).unwrap();
        assert!(
            matches!(source.selected_glyf_loca(None), Err(ReadError::TableIsMissing(tag)) if tag == Tag::new(b"LOCA"))
        );
        assert!(source.glyf().is_ok());
        assert!(source.loca(None).is_ok());
        let mut tables = tables;
        tables.push((*b"LOCA", &[0]));
        let data = font(&tables);
        let source = FontRef::new(&data).unwrap();
        assert!(source.selected_glyf_loca(None).is_err());
    }

    /// https://github.com/googlefonts/fontations/issues/105
    #[test]
    fn bug_105() {
        // serve some dummy versions of the tables used to compute hmtx. The only
        // fields that matter are maxp::num_glyphs and hhea::number_of_h_metrics,
        // everything else is zero'd out
        struct DummyProvider;
        impl TableProvider<'static> for DummyProvider {
            fn data_for_tag(&self, tag: Tag) -> Option<FontData<'static>> {
                if tag == Tag::new(b"maxp") {
                    Some(FontData::new(&[
                        0, 0, 0x50, 0, // version 0.5
                        0, 3, // num_glyphs = 3
                    ]))
                } else if tag == Tag::new(b"hhea") {
                    Some(FontData::new(&[
                        0, 1, 0, 0, // version 1.0
                        0, 0, 0, 0, // ascender/descender
                        0, 0, 0, 0, // line gap/advance width
                        0, 0, 0, 0, // min left/right side bearing
                        0, 0, 0, 0, // x_max, caret_slope_rise
                        0, 0, 0, 0, // caret_slope_run, caret_offset
                        0, 0, 0, 0, // reserved1/2
                        0, 0, 0, 0, // reserved 3/4
                        0, 0, 0, 1, // metric format, number_of_h_metrics
                    ]))
                } else if tag == Tag::new(b"hmtx") {
                    Some(FontData::new(&[
                        0, 4, 0, 6, // LongHorMetric: 4, 6
                        0, 30, 0, 111, // two lsb entries
                    ]))
                } else {
                    None
                }
            }
        }

        let number_of_h_metrics = DummyProvider.hhea().unwrap().number_of_h_metrics();
        let num_glyphs = DummyProvider.maxp().unwrap().num_glyphs();
        let hmtx = DummyProvider.hmtx().unwrap();

        assert_eq!(number_of_h_metrics, 1);
        assert_eq!(num_glyphs, 3);
        assert_eq!(hmtx.h_metrics().len(), 1);
        assert_eq!(hmtx.left_side_bearings().len(), 2);
    }
}
