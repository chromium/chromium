//! Table validation, caching and access.

use super::{super::once::Once, FontBlob, FontSource};
use crate::{
    tables, types::Tag, FontRead, FontReadWithArgs, ReadError, TableProvider, TopLevelTable,
};
use alloc::{boxed::Box, sync::Arc};
use core::sync::atomic::{AtomicU8, Ordering};

include!("../../../data/generated/generated_tables.rs");

/// A single table in a font blob.
#[derive(Default)]
struct BlobTableEntry {
    /// Holds validation state.
    flag: AtomicU8,
    start: u32,
    end: u32,
}

/// Blob and associated metadata for all tables.
struct BlobTables {
    blob: FontBlob,
    tables: Box<PerTableData<BlobTableEntry>>,
}

impl<'a> TableDataProvider<'a> for &'a BlobTables {
    type Entry = BlobTableEntry;

    fn tables(&self) -> &'a PerTableData<Self::Entry> {
        &self.tables
    }

    fn table_state(&self, _tag: Tag, entry: &'a Self::Entry) -> Option<TableState<'a>> {
        self.blob
            .get(entry.start as usize..entry.end as usize)
            .map(|data| TableState {
                flag: &entry.flag,
                data,
            })
    }
}

/// Entry for a single table provided by a callback.
type TableFunctionEntry = Once<Option<(AtomicU8, FontBlob)>>;

/// Lazy, per table font data provided by a function.
#[derive(Clone)]
pub struct FontTableFunction {
    table_fn: Arc<dyn Fn(Tag) -> Option<FontBlob> + Send + Sync>,
    tables: Arc<PerTableData<TableFunctionEntry>>,
}

impl FontTableFunction {
    /// Creates a new font table function with the given callback that should
    /// return a blob containing the table data for the requested tag.
    pub fn new(table_fn: Arc<dyn Fn(Tag) -> Option<FontBlob> + Send + Sync>) -> Self {
        Self {
            table_fn,
            tables: Arc::new(PerTableData::default()),
        }
    }
}

impl<'a> TableDataProvider<'a> for &'a FontTableFunction {
    type Entry = TableFunctionEntry;

    fn tables(&self) -> &'a PerTableData<Self::Entry> {
        &self.tables
    }

    fn table_state(&self, tag: Tag, entry: &'a Self::Entry) -> Option<TableState<'a>> {
        entry
            .get_or_init(|| (self.table_fn)(tag).map(|blob| (AtomicU8::new(0), blob)))
            .as_ref()
            .map(|(flag, data)| TableState {
                flag,
                data: data.as_ref(),
            })
    }
}

/// Source for a set of font tables.
enum TableSource {
    None,
    Blob(BlobTables),
    Function(FontTableFunction),
}

/// Reference to the validation flag and data for a table.
#[derive(Copy, Clone)]
struct TableState<'a> {
    flag: &'a AtomicU8,
    data: &'a [u8],
}

impl<'a> TableState<'a> {
    const UNCHECKED: u8 = 0;
    const VALID: u8 = 1;
    const INVALID: u8 = 2;

    fn try_load<T>(
        &self,
        sanitize_fn: impl Fn(&'a [u8]) -> Result<T, ReadError>,
        load_fn: impl Fn(&'a [u8]) -> Result<T, ReadError>,
    ) -> Result<T, ReadError> {
        match self.flag.load(Ordering::Acquire) {
            Self::UNCHECKED => {
                if let Ok(table) = sanitize_fn(self.data) {
                    self.flag.store(Self::VALID, Ordering::Release);
                    Ok(table)
                } else {
                    self.flag.store(Self::INVALID, Ordering::Release);
                    Err(ReadError::ValidationError)
                }
            }
            Self::VALID => load_fn(self.data),
            // Assume any other bit pattern is invalid
            _ => Err(ReadError::ValidationError),
        }
    }
}

/// Individual font table access.
pub struct FontTables(TableSource);

impl FontTables {
    /// Creates a new set of font tables for the given source.
    pub fn new(source: impl Into<FontSource>, index: u32) -> Result<Self, ReadError> {
        let source = source.into();
        match source {
            FontSource::Blob(blob) => {
                let font_ref = crate::FontRef::from_index(&blob, index)?;
                let mut tables = PerTableData::default();
                let table_records = font_ref.table_directory().table_records();
                tables.init_all(|tag, entry: &mut BlobTableEntry| {
                    // Ensure missing entries report as a missing table rather
                    // than returning an empty slice
                    entry.start = u32::MAX;
                    let Ok(idx) = table_records.binary_search_by_key(&tag, |rec| rec.tag()) else {
                        return;
                    };
                    let record = &table_records[idx];
                    let start = record.offset();
                    let Some(end) = start.checked_add(record.length()) else {
                        return;
                    };
                    if blob.get(start as usize..end as usize).is_none() {
                        return;
                    }
                    *entry = BlobTableEntry {
                        flag: AtomicU8::new(0),
                        start,
                        end,
                    };
                });
                Ok(Self(TableSource::Blob(BlobTables {
                    blob,
                    tables: Box::new(tables),
                })))
            }
            FontSource::TableFunction(func) => Ok(Self(TableSource::Function(func))),
        }
    }
}

impl FontTables {
    fn load_table<'a, T: TopLevelTable + FontRead<'a>>(
        &'a self,
        state: Option<TableState<'a>>,
    ) -> Result<T, ReadError> {
        self.load_table_with_tag(state, T::TAG)
    }

    fn load_table_with_tag<'a, T: FontRead<'a>>(
        &'a self,
        state: Option<TableState<'a>>,
        tag: Tag,
    ) -> Result<T, ReadError> {
        let state = state.ok_or(ReadError::TableIsMissing(tag))?;
        state.try_load(
            |data| FontRead::read(data.into()),
            |data| FontRead::read(data.into()),
        )
    }

    fn load_table_with_args<'a, T: TopLevelTable + FontReadWithArgs<'a>>(
        &'a self,
        state: Option<TableState<'a>>,
        args: &T::Args,
    ) -> Result<T, ReadError> {
        let state = state.ok_or(ReadError::TableIsMissing(T::TAG))?;
        state.try_load(
            |data| FontReadWithArgs::read_with_args(data.into(), args),
            |data| FontReadWithArgs::read_with_args(data.into(), args),
        )
    }
}

pub(super) static EMPTY_FONT_TABLES: FontTables = FontTables(TableSource::None);

impl<'a> TableProvider<'a> for &'a FontTables {
    fn data_for_tag(&self, _tag: Tag) -> Option<crate::FontData<'a>> {
        None
    }

    fn head(&self) -> Result<tables::head::Head<'a>, ReadError> {
        self.load_table(self.head_state())
    }

    fn name(&self) -> Result<tables::name::Name<'a>, ReadError> {
        self.load_table(self.name_state())
    }

    fn hhea(&self) -> Result<tables::hhea::Hhea<'a>, ReadError> {
        self.load_table(self.hhea_state())
    }

    fn vhea(&self) -> Result<tables::vhea::Vhea<'a>, ReadError> {
        self.load_table(self.vhea_state())
    }

    fn hmtx(&self) -> Result<tables::hmtx::Hmtx<'a>, ReadError> {
        //FIXME: should we make the user pass these in?
        let number_of_h_metrics = self.hhea().map(|hhea| hhea.number_of_h_metrics())?;
        self.load_table_with_args(self.hmtx_state(), &number_of_h_metrics)
    }

    fn hdmx(&self) -> Result<tables::hdmx::Hdmx<'a>, ReadError> {
        let num_glyphs = self.maxp().map(|maxp| maxp.num_glyphs())?;
        self.load_table_with_args(self.hdmx_state(), &num_glyphs)
    }

    fn vmtx(&self) -> Result<tables::vmtx::Vmtx<'a>, ReadError> {
        //FIXME: should we make the user pass these in?
        let number_of_v_metrics = self.vhea().map(|vhea| vhea.number_of_long_ver_metrics())?;
        self.load_table_with_args(self.vmtx_state(), &number_of_v_metrics)
    }

    fn vorg(&self) -> Result<tables::vorg::Vorg<'a>, ReadError> {
        self.load_table(self.vorg_state())
    }

    fn fvar(&self) -> Result<tables::fvar::Fvar<'a>, ReadError> {
        self.load_table(self.fvar_state())
    }

    fn avar(&self) -> Result<tables::avar::Avar<'a>, ReadError> {
        self.load_table(self.avar_state())
    }

    fn hvar(&self) -> Result<tables::hvar::Hvar<'a>, ReadError> {
        self.load_table(self.hvar_state())
    }

    fn vvar(&self) -> Result<tables::vvar::Vvar<'a>, ReadError> {
        self.load_table(self.vvar_state())
    }

    fn mvar(&self) -> Result<tables::mvar::Mvar<'a>, ReadError> {
        self.load_table(self.mvar_state())
    }

    fn maxp(&self) -> Result<tables::maxp::Maxp<'a>, ReadError> {
        self.load_table(self.maxp_state())
    }

    fn os2(&self) -> Result<tables::os2::Os2<'a>, ReadError> {
        self.load_table(self.os2_state())
    }

    fn post(&self) -> Result<tables::post::Post<'a>, ReadError> {
        self.load_table(self.post_state())
    }

    fn gasp(&self) -> Result<tables::gasp::Gasp<'a>, ReadError> {
        self.load_table(self.gasp_state())
    }

    /// is_long can be optionally provided, if known, otherwise we look it up in head.
    fn loca(&self, is_long: impl Into<Option<bool>>) -> Result<tables::loca::Loca<'a>, ReadError> {
        let is_long = match is_long.into() {
            Some(val) => val,
            None => self.head()?.index_to_loc_format() == 1,
        };
        self.load_table_with_args(self.loca_state(), &is_long)
    }

    fn glyf(&self) -> Result<tables::glyf::Glyf<'a>, ReadError> {
        self.load_table(self.glyf_state())
    }

    fn gvar(&self) -> Result<tables::gvar::Gvar<'a>, ReadError> {
        self.load_table(self.gvar_state())
    }

    /// Returns the array of entries for the control value table which is used
    /// for TrueType hinting.
    fn cvt(&self) -> Result<&'a [types::BigEndian<i16>], ReadError> {
        let table_data = crate::FontData::new(
            self.cvt_data()
                .ok_or(ReadError::TableIsMissing(Tag::new(b"cvt ")))?,
        );
        table_data.read_array(0..table_data.len())
    }

    fn cvar(&self) -> Result<tables::cvar::Cvar<'a>, ReadError> {
        self.load_table(self.cvar_state())
    }

    fn cff(&self) -> Result<tables::cff::Cff<'a>, ReadError> {
        self.load_table(self.cff_state())
    }

    fn cff2(&self) -> Result<tables::cff2::Cff2<'a>, ReadError> {
        self.load_table(self.cff2_state())
    }

    fn cmap(&self) -> Result<tables::cmap::Cmap<'a>, ReadError> {
        self.load_table(self.cmap_state())
    }

    fn gdef(&self) -> Result<tables::gdef::Gdef<'a>, ReadError> {
        self.load_table(self.gdef_state())
    }

    fn gpos(&self) -> Result<tables::gpos::Gpos<'a>, ReadError> {
        self.load_table(self.gpos_state())
    }

    fn gsub(&self) -> Result<tables::gsub::Gsub<'a>, ReadError> {
        self.load_table(self.gsub_state())
    }

    fn feat(&self) -> Result<tables::feat::Feat<'a>, ReadError> {
        self.load_table(self.feat_state())
    }

    fn ltag(&self) -> Result<tables::ltag::Ltag<'a>, ReadError> {
        self.load_table(self.ltag_state())
    }

    fn ankr(&self) -> Result<tables::ankr::Ankr<'a>, ReadError> {
        self.load_table(self.ankr_state())
    }

    fn trak(&self) -> Result<tables::trak::Trak<'a>, ReadError> {
        self.load_table(self.trak_state())
    }

    fn morx(&self) -> Result<tables::morx::Morx<'a>, ReadError> {
        self.load_table(self.morx_state())
    }

    fn kerx(&self) -> Result<tables::kerx::Kerx<'a>, ReadError> {
        self.load_table(self.kerx_state())
    }

    fn kern(&self) -> Result<tables::kern::Kern<'a>, ReadError> {
        self.load_table(self.kern_state())
    }

    fn colr(&self) -> Result<tables::colr::Colr<'a>, ReadError> {
        self.load_table(self.colr_state())
    }

    fn cpal(&self) -> Result<tables::cpal::Cpal<'a>, ReadError> {
        self.load_table(self.cpal_state())
    }

    fn cblc(&self) -> Result<tables::cblc::Cblc<'a>, ReadError> {
        self.load_table(self.cblc_state())
    }

    fn cbdt(&self) -> Result<tables::cbdt::Cbdt<'a>, ReadError> {
        self.load_table(self.cbdt_state())
    }

    fn eblc(&self) -> Result<tables::eblc::Eblc<'a>, ReadError> {
        self.load_table(self.eblc_state())
    }

    fn ebdt(&self) -> Result<tables::ebdt::Ebdt<'a>, ReadError> {
        self.load_table(self.ebdt_state())
    }

    fn sbix(&self) -> Result<tables::sbix::Sbix<'a>, ReadError> {
        // should we make the user pass this in?
        let num_glyphs = self.maxp().map(|maxp| maxp.num_glyphs())?;
        self.load_table_with_args(self.sbix_state(), &num_glyphs)
    }

    fn stat(&self) -> Result<tables::stat::Stat<'a>, ReadError> {
        self.load_table(self.stat_state())
    }

    fn svg(&self) -> Result<tables::svg::Svg<'a>, ReadError> {
        self.load_table(self.svg_state())
    }

    fn varc(&self) -> Result<tables::varc::Varc<'a>, ReadError> {
        self.load_table(self.varc_state())
    }

    #[cfg(feature = "ift")]
    fn ift(&self) -> Result<tables::ift::Ift<'a>, ReadError> {
        self.load_table_with_tag(self.ift_state(), Tag::new(b"IFT "))
    }

    #[cfg(feature = "ift")]
    fn iftx(&self) -> Result<tables::ift::Ift<'a>, ReadError> {
        self.load_table_with_tag(self.iftx_state(), Tag::new(b"IFTX"))
    }

    fn meta(&self) -> Result<tables::meta::Meta<'a>, ReadError> {
        self.load_table(self.meta_state())
    }

    fn base(&self) -> Result<tables::base::Base<'a>, ReadError> {
        self.load_table(self.base_state())
    }

    fn dsig(&self) -> Result<tables::dsig::Dsig<'a>, ReadError> {
        self.load_table(self.dsig_state())
    }
}

#[cfg(test)]
mod tests {
    use crate::FontRef;

    use super::*;

    #[test]
    fn missing_tables_are_missing() {
        let tables = &FontTables::new(font_test_data::AHEM, 0).unwrap();
        assert!(matches!(tables.cbdt(), Err(ReadError::TableIsMissing(_))));
        assert!(matches!(tables.cff(), Err(ReadError::TableIsMissing(_))));
        assert!(matches!(tables.dsig(), Err(ReadError::TableIsMissing(_))));
    }

    #[test]
    fn table_function_matches_font_ref() {
        let font = FontRef::new(font_test_data::AHEM).unwrap();
        let font_copy = font.clone();
        let table_fn = FontTableFunction::new(Arc::new(move |tag| {
            font_copy
                .data_for_tag(tag)
                .map(|data| Vec::from(data.as_bytes()).into())
        }));
        let tables = &FontTables::new(table_fn, 0).unwrap();
        for (data, tag) in [
            (tables.gasp_data(), b"gasp"),
            (tables.glyf_data(), b"glyf"),
            (tables.hmtx_data(), b"hmtx"),
        ] {
            let font_ref_data = font.data_for_tag(Tag::new(tag)).map(|d| d.as_bytes());
            assert_eq!(data, font_ref_data);
        }
    }
}
