//! Tables parsed once for a font and kept.
//!
//! A parsed table borrows the bytes it came from, so it cannot simply be
//! stored beside them. [`Yoke`] makes that expressible by holding the parsed
//! value together with the tables it borrows, behind an [`Arc`] that outlives
//! both however they are dropped.
//!
//! Every table gets its own cell, so reading one never drags in another.
//! That matters for `glyf` and `loca`, which outlines and metrics in both
//! directions all read, and for `gvar`, which is typically about half a
//! variable font and must not be read to answer what another table can.

use super::Tables;
use crate::model::glyph::outline::varc;
use crate::ps::cff::CffFontRef;
use crate::tables::{
    cbdt::Cbdt,
    cblc::Cblc,
    colr::Colr,
    cpal::Cpal,
    ebdt::Ebdt,
    eblc::Eblc,
    glyf::Glyf,
    gvar::SelectedGvar,
    hmtx::GlyphMetricRecords,
    hvar::Hvar,
    loca::Loca,
    sbix::Sbix,
    variations::{DeltaSetIndexMap, ItemVariationStore},
    vorg::Vorg,
    vvar::Vvar,
};
use crate::TableProvider;

use alloc::sync::Arc;
use yoke::{Yoke, Yokeable};

/// Returns the design units per em that the font's metrics use.
fn units_per_em<'a>(tables: &impl TableProvider<'a>) -> u16 {
    crate::model::metrics::units_per_em(tables.head().map_or(0, |head| head.units_per_em()))
}

/// A parsed table held beside the tables it borrows.
pub(crate) struct TableCache<Y: for<'a> Yokeable<'a>>(Yoke<Y, Arc<Tables>>);

impl<Y: for<'a> Yokeable<'a>> TableCache<Y> {
    /// Parses once, with `read`, and keeps the result.
    pub(crate) fn read<F>(tables: Arc<Tables>, read: F) -> Self
    where
        F: for<'a> FnOnce(&'a Tables) -> <Y as Yokeable<'a>>::Output,
    {
        Self(Yoke::attach_to_cart(tables, read))
    }

    /// Returns the parsed table, borrowed for no longer than this.
    #[inline]
    pub(crate) fn get(&self) -> &<Y as Yokeable<'_>>::Output {
        self.0.get()
    }
}

/// The outline tables.
///
/// These are read as a pair because neither is usable alone: `loca` gives
/// the offset of each glyph in `glyf`.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct GlyfLoca<'a>(pub(crate) Option<(Glyf<'a>, Loca<'a>)>);

impl<'a> GlyfLoca<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.selected_glyf_loca(None).ok())
    }
}

/// The glyph metrics of one direction.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct MetricsTable<'a>(pub(crate) GlyphMetricRecords<'a>);

/// The table of horizontal metric variations.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct HvarTable<'a>(pub(crate) Option<Hvar<'a>>);

impl<'a> HvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.hvar().ok())
    }
}

/// The table of vertical metric variations.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct VvarTable<'a>(pub(crate) Option<Vvar<'a>>);

impl<'a> VvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.vvar().ok())
    }
}

/// The table that varies `glyf` outlines.
///
/// Outlines read it at a location away from the default. Metrics read it
/// where `HVAR` or `VVAR` is absent, from the phantom points of each glyph.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct GvarTable<'a>(pub(crate) Option<SelectedGvar<'a>>);

impl<'a> GvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.selected_gvar().ok())
    }
}

/// Charstring outlines from the `CFF2` table.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct Cff2Font<'a>(pub(crate) Option<CffFontRef<'a>>);

impl<'a> Cff2Font<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        let upem = Some(units_per_em(tables) as i32);
        Self(
            tables
                .cff2()
                .ok()
                .and_then(|cff2| CffFontRef::new(cff2.offset_data().as_bytes(), 0, upem).ok()),
        )
    }
}

/// Charstring outlines from the `CFF` table.
///
/// This is separate from [`Cff2Font`] so that a caller can choose either
/// table when a font has both.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct CffFont<'a>(pub(crate) Option<CffFontRef<'a>>);

impl<'a> CffFont<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        let upem = Some(units_per_em(tables) as i32);
        Self(
            tables
                .cff()
                .ok()
                .and_then(|cff| CffFontRef::new(cff.offset_data().as_bytes(), 0, upem).ok()),
        )
    }
}

/// The table of glyph vertical origins.
///
/// Used only for vertical text, where it takes precedence over every other
/// source of the origin.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct VorgTable<'a>(pub(crate) Option<Vorg<'a>>);

impl<'a> VorgTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.vorg().ok())
    }
}

impl<'a> varc::Outlines<'a> {
    /// Reads the variable composites, with their coverage and variation
    /// store resolved.
    ///
    /// Returns `None` if the font has no `VARC` table.
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Option<Self> {
        let varc = tables.varc().ok()?;
        let upem = units_per_em(tables);
        // A static VARC font may omit fvar and use the axes retained in gvar
        // or the CFF2 variation store for component-internal coordinates.
        let mut axis_count = tables
            .fvar()
            .map(|fvar| fvar.axis_count() as usize)
            .unwrap_or_default();
        if let Ok(gvar) = tables.gvar() {
            axis_count = axis_count.max(gvar.axis_count() as usize);
        }
        if let Ok(gvar) = tables.gvar_extended() {
            axis_count = axis_count.max(gvar.axis_count() as usize);
        }
        if let Some(count) = tables
            .cff2()
            .ok()
            .and_then(|cff2| CffFontRef::new(cff2.offset_data().as_bytes(), 0, None).ok())
            .and_then(|cff| cff.var_store()?.variation_region_list().ok())
            .map(|regions| regions.axis_count() as usize)
        {
            axis_count = axis_count.max(count);
        }
        varc::Outlines::new(varc, upem, axis_count).ok()
    }
}

/// The `COLR` table and the variation tables its paints use.
#[derive(Clone, Yokeable)]
pub(crate) struct ColrTables<'a> {
    pub(crate) colr: Colr<'a>,
    pub(crate) index_map: Option<DeltaSetIndexMap<'a>>,
    pub(crate) var_store: Option<ItemVariationStore<'a>>,
}

impl<'a> ColrTables<'a> {
    /// Reads the tables, or returns `None` if the font has no `COLR` table.
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Option<Self> {
        tables.colr().ok().map(Self::new)
    }

    pub(crate) fn new(colr: Colr<'a>) -> Self {
        Self {
            index_map: colr.var_index_map().and_then(|map| map.ok()),
            var_store: colr.item_variation_store().and_then(|store| store.ok()),
            colr,
        }
    }
}

/// The color palettes in `CPAL`.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct CpalTable<'a>(pub(crate) Option<Cpal<'a>>);

impl<'a> CpalTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.cpal().ok())
    }
}

/// PNG bitmaps in `sbix`.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct SbixTable<'a>(pub(crate) Option<Sbix<'a>>);

impl<'a> SbixTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.sbix().ok())
    }
}

/// Color bitmaps in `CBDT`, with the `CBLC` that locates them.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct CbdtTables<'a>(pub(crate) Option<(Cblc<'a>, Cbdt<'a>)>);

impl<'a> CbdtTables<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.cblc().ok().zip(tables.cbdt().ok()))
    }
}

/// Monochrome and grayscale bitmaps in `EBDT`, with the `EBLC` that locates
/// them.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct EbdtTables<'a>(pub(crate) Option<(Eblc<'a>, Ebdt<'a>)>);

impl<'a> EbdtTables<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.eblc().ok().zip(tables.ebdt().ok()))
    }
}
