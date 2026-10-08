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
use crate::ps::cff::CffFontRef;
use crate::tables::{glyf::Glyf, gvar::Gvar, hvar::Hvar, loca::Loca, vorg::Vorg, vvar::Vvar};
use crate::TableProvider;
use alloc::sync::Arc;
use yoke::{Yoke, Yokeable};

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
/// These are read as a pair because neither is usable alone: `loca` states
/// where in `glyf` a glyph begins.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct GlyfLoca<'a>(pub(crate) Option<(Glyf<'a>, Loca<'a>)>);

impl<'a> GlyfLoca<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.glyf().ok().zip(tables.loca(None).ok()))
    }
}

/// The table stating how a location changes horizontal metrics.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct HvarTable<'a>(pub(crate) Option<Hvar<'a>>);

impl<'a> HvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.hvar().ok())
    }
}

/// The table stating how a location changes vertical metrics.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct VvarTable<'a>(pub(crate) Option<Vvar<'a>>);

impl<'a> VvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.vvar().ok())
    }
}

/// The table stating how a location changes outlines.
///
/// Metrics read this only where `HVAR` is absent, recovering the answer from
/// the phantom points it carries alongside each glyph.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct GvarTable<'a>(pub(crate) Option<Gvar<'a>>);

impl<'a> GvarTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.gvar().ok())
    }
}

/// The outlines of a font that states them as charstrings.
///
/// `CFF2` first, as HarfBuzz reads them: a font carrying both states its
/// variable outlines there.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct CffFont<'a>(pub(crate) Option<CffFontRef<'a>>);

impl<'a> CffFont<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        let upem = tables.head().ok().map(|head| head.units_per_em() as i32);
        let data = tables
            .cff2()
            .ok()
            .map(|cff2| cff2.offset_data().as_bytes())
            .or_else(|| tables.cff().ok().map(|cff| cff.offset_data().as_bytes()));
        Self(data.and_then(|data| CffFontRef::new(data, 0, upem).ok()))
    }
}

/// The table stating where a glyph's vertical origin sits.
///
/// Read only by vertical text, and only ahead of everything else that could
/// answer: a font stating this states it outright.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct VorgTable<'a>(pub(crate) Option<Vorg<'a>>);

impl<'a> VorgTable<'a> {
    pub(crate) fn read(tables: &impl TableProvider<'a>) -> Self {
        Self(tables.vorg().ok())
    }
}
