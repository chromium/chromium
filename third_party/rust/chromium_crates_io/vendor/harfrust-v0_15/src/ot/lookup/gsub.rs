//! Substitution subtables.
//!
//! Each kind serves the original format and its extended counterpart, which
//! differ only in the widths of glyph IDs and offsets.

use super::coverage::{Coverage, CoverageArray};
use super::{read_be, Offsets, SubtableDigests, Uints, Values};
use crate::budget::{Budget, Exhausted};
use crate::buffer::{GlyphInfo, GlyphPropsFlags};
use crate::hint::Likely;
use crate::ot::apply::{
    ligate_input, match_always, match_backtrack, match_glyph, match_input, match_lookahead,
    ApplyContext, MaySkip, SkippingIterator, WouldApplyContext,
};
use crate::ot::layout::MAX_NESTING_LEVEL;
use crate::ot::map::OtMap;
use crate::set_digest::SetDigest;
#[cfg(feature = "tracing")]
use alloc::string::String;
use read_fonts::tables::gsub::{
    AlternateSubstFormat1, AlternateSubstFormat2, LigatureSubstFormat1, LigatureSubstFormat2,
    MultipleSubstFormat1, MultipleSubstFormat2, ReverseChainSingleSubstFormat1,
    ReverseChainSingleSubstFormat2, SingleSubstFormat1, SingleSubstFormat2, SingleSubstFormat3,
    SingleSubstFormat4,
};
use read_fonts::types::GlyphId;
use read_fonts::FontData;

/// The width of a glyph ID in the original formats.
const NARROW: usize = 2;
/// The width of a glyph ID, and of a child offset, in the extended formats.
const WIDE: usize = 3;

/// A count of 16 bits followed by that many glyph IDs of `width` bytes, as
/// in a sequence or alternate set.
fn counted_glyphs(data: FontData, width: usize) -> Option<Uints> {
    let count = usize::from(data.read_at::<u16>(0).ok()?);
    Uints::at(data, 2, count, width)
}

/// Single substitution by a fixed delta, wrapping within the glyph ID width.
pub(crate) struct SingleSubst1 {
    delta: i32,
    mask: u32,
}

impl SingleSubst1 {
    pub fn new(table: &SingleSubstFormat1) -> Option<Self> {
        Some(Self {
            delta: table.delta_glyph_id().into(),
            mask: 0xFFFF,
        })
    }

    pub fn new_wide(table: &SingleSubstFormat3) -> Option<Self> {
        Some(Self {
            delta: table.delta_glyph_id().to_i32(),
            mask: 0xFF_FFFF,
        })
    }

    pub fn apply(&self, ctx: &mut ApplyContext, _index: u32) -> Option<()> {
        let glyph = ctx.buffer.cur(0)?.glyph_id;
        let substitute = (glyph as i32).wrapping_add(self.delta) as u32 & self.mask;
        message!(
            ctx,
            "replacing glyph at {} (single substitution)",
            ctx.buffer.message_idx()
        );
        ctx.replace_glyph(substitute.into());
        message!(
            ctx,
            "replaced glyph at {} (single substitution)",
            ctx.buffer.message_idx() - 1
        );
        Some(())
    }
}

impl SubtableDigests<'_> for SingleSubst1 {}

/// Single substitution from an array of substitutes.
pub(crate) struct SingleSubst2<'a> {
    substitutes: Uints<'a>,
}

impl<'a> SingleSubst2<'a> {
    pub fn new(table: &SingleSubstFormat2<'a>) -> Option<Self> {
        let range = table.substitute_glyph_ids_byte_range();
        Some(Self {
            substitutes: Uints::new(table.offset_data(), range, NARROW)?,
        })
    }

    pub fn new_wide(table: &SingleSubstFormat4<'a>) -> Option<Self> {
        let range = table.substitute_glyph_ids_byte_range();
        Some(Self {
            substitutes: Uints::new(table.offset_data(), range, WIDE)?,
        })
    }

    pub fn apply(&self, ctx: &mut ApplyContext, index: u32) -> Option<()> {
        let substitute = self.substitutes.get(index as usize)?;
        message!(
            ctx,
            "replacing glyph at {} (single substitution)",
            ctx.buffer.message_idx()
        );
        ctx.replace_glyph(substitute.into());
        message!(
            ctx,
            "replaced glyph at {} (single substitution)",
            ctx.buffer.message_idx() - 1
        );
        Some(())
    }
}

impl<'a> SubtableDigests<'a> for SingleSubst2<'a> {}

/// Multiple substitution: one glyph into a sequence.
pub(crate) struct MultipleSubst1<'a> {
    sequences: Offsets<'a>,
    glyph_width: u8,
}

impl<'a> MultipleSubst1<'a> {
    pub fn new(table: &MultipleSubstFormat1<'a>) -> Option<Self> {
        let range = table.sequence_offsets_byte_range();
        Some(Self {
            sequences: Offsets::new(table.offset_data(), range, NARROW),
            glyph_width: NARROW as u8,
        })
    }

    pub fn new_wide(table: &MultipleSubstFormat2<'a>) -> Option<Self> {
        let range = table.sequence_offsets_byte_range();
        Some(Self {
            sequences: Offsets::new(table.offset_data(), range, WIDE),
            glyph_width: WIDE as u8,
        })
    }

    pub fn apply(&self, ctx: &mut ApplyContext, index: u32) -> Option<()> {
        let data = self.sequences.data(index as usize)?;
        let substitutes = counted_glyphs(data, self.glyph_width.into())?;
        match substitutes.len() {
            // The spec disallows empty sequences, but Uniscribe deletes the
            // glyph. https://github.com/harfbuzz/harfbuzz/issues/253
            0 => {
                message!(
                    ctx,
                    "deleting glyph at {} (multiple substitution)",
                    ctx.buffer.message_idx()
                );
                ctx.buffer.delete_glyph();
                message!(
                    ctx,
                    "deleted glyph at {} (multiple substitution)",
                    ctx.buffer.message_idx()
                );
            }
            // A single substitute is an in-place replacement, not a
            // multiplication.
            1 => {
                message!(
                    ctx,
                    "replacing glyph at {} (multiple substitution)",
                    ctx.buffer.message_idx()
                );
                ctx.replace_glyph(substitutes.first()?.into());
                message!(
                    ctx,
                    "replaced glyph at {} (multiple substitution)",
                    ctx.buffer.message_idx() - 1
                );
            }
            _ => {
                message!(ctx, "multiplying glyph at {}", ctx.buffer.message_idx());
                let class = if ctx.buffer.cur(0)?.is_ligature() {
                    GlyphPropsFlags::BASE_GLYPH
                } else {
                    GlyphPropsFlags::empty()
                };
                let lig_id = ctx.buffer.cur(0)?.lig_id();
                for (component, substitute) in substitutes.iter().enumerate() {
                    // Leave a glyph attached to a ligature as it is.
                    // https://github.com/harfbuzz/harfbuzz/issues/3069
                    if lig_id == 0 {
                        // The component is stored in four bits.
                        ctx.buffer
                            .cur_mut(0)?
                            .set_lig_props_for_component(component as u8);
                    }
                    ctx.output_glyph_for_component(substitute.into(), class);
                }
                ctx.buffer.skip_glyph();
                #[cfg(feature = "tracing")]
                if ctx.buffer.messaging() && ctx.buffer.successful {
                    let count = substitutes.len();
                    let end = ctx.buffer.message_idx();
                    let mut msg = String::from("multiplied glyphs at ");
                    for i in (end - count)..end {
                        if i > (end - count) {
                            msg.push(',');
                        }
                        use core::fmt::Write;
                        let _ = write!(msg, "{i}");
                    }
                    ctx.buffer.message(ctx.font, &msg);
                }
            }
        }
        Some(())
    }
}

impl<'a> SubtableDigests<'a> for MultipleSubst1<'a> {}

/// Alternate substitution, chosen by the feature value.
pub(crate) struct AlternateSubst1<'a> {
    alternate_sets: Offsets<'a>,
    glyph_width: u8,
}

impl<'a> AlternateSubst1<'a> {
    pub fn new(table: &AlternateSubstFormat1<'a>) -> Option<Self> {
        let range = table.alternate_set_offsets_byte_range();
        Some(Self {
            alternate_sets: Offsets::new(table.offset_data(), range, NARROW),
            glyph_width: NARROW as u8,
        })
    }

    pub fn new_wide(table: &AlternateSubstFormat2<'a>) -> Option<Self> {
        let range = table.alternate_set_offsets_byte_range();
        Some(Self {
            alternate_sets: Offsets::new(table.offset_data(), range, WIDE),
            glyph_width: WIDE as u8,
        })
    }

    pub fn apply(&self, ctx: &mut ApplyContext, index: u32) -> Option<()> {
        let data = self.alternate_sets.data(index as usize)?;
        apply_alternate_set(ctx, counted_glyphs(data, self.glyph_width.into())?)
    }
}

impl<'a> SubtableDigests<'a> for AlternateSubst1<'a> {}

fn apply_alternate_set(ctx: &mut ApplyContext, alternates: Uints) -> Option<()> {
    if alternates.is_empty() {
        return None;
    }
    let glyph_mask = ctx.buffer.cur(0)?.mask;
    let lookup_mask = ctx.lookup_mask();
    // This breaks if two features enable the lookup together.
    let mut alt_index = (lookup_mask & glyph_mask) >> lookup_mask.trailing_zeros();
    // The maximum value picks a random alternate under the `rand` feature.
    if alt_index == OtMap::MAX_VALUE && ctx.random {
        // Changing the random state makes every position unsafe to break.
        ctx.buffer.unsafe_to_break(Some(0), Some(ctx.buffer.len));
        alt_index = ctx.random_number() % alternates.len() as u32 + 1;
    }
    let index = u16::try_from(alt_index).ok()?.checked_sub(1)?;
    message!(
        ctx,
        "replacing glyph at {} (alternate substitution)",
        ctx.buffer.message_idx()
    );
    ctx.replace_glyph(alternates.get(index as usize)?.into());
    message!(
        ctx,
        "replaced glyph at {} (alternate substitution)",
        ctx.buffer.message_idx() - 1
    );
    Some(())
}

/// A ligature: its glyph and its components after the first, whose glyph
/// IDs are `W` bytes.
struct Ligature<'a, const W: usize> {
    glyph: u32,
    components: Values<'a, W>,
}

impl<'a, const W: usize> Ligature<'a, W> {
    /// Reads a ligature; `None` if it is truncated.
    #[inline(always)]
    fn read(data: FontData<'a>) -> Option<Self> {
        let glyph = read_be::<W>(data.as_bytes(), 0)?;
        let count = usize::from(data.read_at::<u16>(W).ok()?);
        Some(Self {
            glyph,
            components: Values::at(data, W + 2, count.saturating_sub(1))?,
        })
    }
}

/// The ligatures of a ligature set, read on demand. Its offsets and glyph
/// IDs are `W` bytes.
struct LigatureSet<'a, const W: usize> {
    data: FontData<'a>,
    offsets: Values<'a, W>,
}

impl<'a, const W: usize> LigatureSet<'a, W> {
    #[inline(always)]
    fn read(data: FontData<'a>) -> Option<Self> {
        let count = usize::from(data.read_at::<u16>(0).ok()?);
        Some(Self {
            data,
            offsets: Values::at(data, 2, count)?,
        })
    }

    fn len(&self) -> usize {
        self.offsets.len()
    }

    /// The readable ligatures, in order; a null offset is skipped.
    #[inline(always)]
    fn iter(&self) -> impl Iterator<Item = Ligature<'a, W>> + '_ {
        self.offsets.iter().filter_map(|offset| {
            if offset == 0 {
                return None;
            }
            Ligature::read(self.data.split_off(offset as usize)?)
        })
    }
}

/// Ligature substitution.
pub(crate) struct LigatureSubst1<'a> {
    /// The second components of all ligatures.
    seconds: SetDigest,
    ligature_sets: Offsets<'a>,
    width: u8,
}

impl<'a> LigatureSubst1<'a> {
    pub fn new(table: &LigatureSubstFormat1<'a>, budget: &Budget) -> Result<Self, Exhausted> {
        let range = table.ligature_set_offsets_byte_range();
        Self::from_sets(
            Offsets::new(table.offset_data(), range, NARROW),
            NARROW,
            budget,
        )
    }

    pub fn new_wide(table: &LigatureSubstFormat2<'a>, budget: &Budget) -> Result<Self, Exhausted> {
        let range = table.ligature_set_offsets_byte_range();
        Self::from_sets(Offsets::new(table.offset_data(), range, WIDE), WIDE, budget)
    }

    fn from_sets(
        ligature_sets: Offsets<'a>,
        width: usize,
        budget: &Budget,
    ) -> Result<Self, Exhausted> {
        let seconds = if width == WIDE {
            collect_seconds::<WIDE>(&ligature_sets, budget)?
        } else {
            collect_seconds::<NARROW>(&ligature_sets, budget)?
        };
        Ok(Self {
            seconds,
            ligature_sets,
            width: width as u8,
        })
    }

    // Each width gets its own copy of the ligature walk, which reads many
    // glyph IDs per application.
    pub fn apply(&self, ctx: &mut ApplyContext, index: u32) -> Option<()> {
        let data = self.ligature_sets.data(index as usize)?;
        if usize::from(self.width) == WIDE {
            apply_ligature_set(ctx, &LigatureSet::<WIDE>::read(data)?, &self.seconds)
        } else {
            apply_ligature_set(ctx, &LigatureSet::<NARROW>::read(data)?, &self.seconds)
        }
    }

    pub fn would_apply(&self, ctx: &WouldApplyContext, coverage: &Coverage) -> bool {
        let Some(data) = coverage
            .get(ctx.glyphs[0])
            .and_then(|index| self.ligature_sets.data(index as usize))
        else {
            return false;
        };
        if usize::from(self.width) == WIDE {
            set_would_apply(LigatureSet::<WIDE>::read(data), ctx)
        } else {
            set_would_apply(LigatureSet::<NARROW>::read(data), ctx)
        }
    }
}

fn set_would_apply<const W: usize>(set: Option<LigatureSet<W>>, ctx: &WouldApplyContext) -> bool {
    set.is_some_and(|set| {
        set.iter()
            .any(|ligature| ligature_would_apply(&ligature, ctx))
    })
}

impl<'a> SubtableDigests<'a> for LigatureSubst1<'a> {
    const CACHES_COVERAGE: bool = true;

    fn digest_second(&self, _budget: &Budget) -> Result<SetDigest, Exhausted> {
        Ok(self.seconds.clone())
    }
}

/// The digest of every ligature's second glyph, or a full digest if a
/// ligature has none. Nested walks share the font's preprocessing allowance.
fn collect_seconds<const W: usize>(
    sets: &Offsets,
    budget: &Budget,
) -> Result<SetDigest, Exhausted> {
    let mut seconds = SetDigest::new();
    budget.work(sets.len())?;
    for index in 0..sets.len() {
        let Some(set) = sets.data(index).and_then(LigatureSet::<W>::read) else {
            continue;
        };
        budget.work(set.len())?;
        for ligature in set.iter() {
            let Some(second) = ligature.components.first() else {
                return Ok(SetDigest::full());
            };
            seconds.add(second);
        }
    }
    Ok(seconds)
}

fn ligature_would_apply<const W: usize>(ligature: &Ligature<W>, ctx: &WouldApplyContext) -> bool {
    let components = &ligature.components;
    ctx.glyphs.len() == components.len() + 1
        && components
            .iter()
            .zip(&ctx.glyphs[1..])
            .all(|(component, glyph)| GlyphId::new(component) == *glyph)
}

/// Applies the first ligature of `set` that matches.
fn apply_ligature_set<const W: usize>(
    ctx: &mut ApplyContext,
    set: &LigatureSet<W>,
    seconds: &SetDigest,
) -> Option<()> {
    // Find the glyph after the current one, to reject ligatures by their
    // first component without matching them.
    let mut second = None;
    let mut unsafe_to = 0;
    if set.len() > 1 {
        let mut iter = SkippingIterator::with_match_fn(ctx, true, Some(match_always));
        if iter.reset(iter.buffer.idx).is_none() {
            iter.buffer.index_failed();
        }
        if iter.next(Some(&mut unsafe_to)) {
            let info = iter.buffer.info.get(iter.index()).likely()?;
            unsafe_to = iter.index() + 1;
            // A skippable next glyph, such as a default ignorable, may not
            // be the second component.
            if iter.may_skip(info) == MaySkip::No {
                second = Some(info.glyph_id);
            }
        }
    }

    let Some(second) = second else {
        return set
            .iter()
            .find_map(|ligature| apply_ligature(ctx, &ligature));
    };
    if !seconds.may_have(second) {
        // Every ligature here has a second component and none of them is
        // this glyph; `collect_seconds` gives up on any ligature without
        // one.
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
        return None;
    }
    let mut unsafe_to_concat = false;
    for ligature in set.iter() {
        match ligature.components.first() {
            Some(first) if first != second => unsafe_to_concat = true,
            _ => {
                if apply_ligature(ctx, &ligature).is_some() {
                    if unsafe_to_concat {
                        ctx.buffer
                            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
                    }
                    return Some(());
                }
            }
        }
    }
    if unsafe_to_concat {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(unsafe_to));
    }
    None
}

fn apply_ligature<const W: usize>(ctx: &mut ApplyContext, ligature: &Ligature<W>) -> Option<()> {
    let components = &ligature.components;
    let ligature_glyph = ligature.glyph.into();
    // A ligature of one glyph is an in-place replacement, not a ligation.
    if components.is_empty() {
        message!(
            ctx,
            "replacing glyph at {} (ligature substitution)",
            ctx.buffer.message_idx()
        );
        ctx.replace_glyph(ligature_glyph);
        message!(
            ctx,
            "replaced glyph at {} (ligature substitution)",
            ctx.buffer.message_idx() - 1
        );
        return Some(());
    }
    let matches = |info: &mut GlyphInfo, index: u32| {
        components
            .get(index as usize)
            .is_some_and(|component| match_glyph(info, component))
    };
    let mut match_end = 0;
    let mut total_component_count = 0;
    if !match_input(
        ctx,
        components.len() as u16,
        matches,
        &mut match_end,
        Some(&mut total_component_count),
    )? {
        ctx.buffer
            .unsafe_to_concat(Some(ctx.buffer.idx), Some(match_end));
        return None;
    }
    #[cfg(feature = "tracing")]
    let mut pos = 0;
    #[cfg(feature = "tracing")]
    if ctx.buffer.messaging() {
        pos = ctx.buffer.message_idx();
        let mut msg = String::from("ligating glyphs at ");
        for i in 0..components.len() + 1 {
            let index = ctx.match_positions[i] as usize - ctx.buffer.idx + pos;
            if i > 0 {
                msg.push(',');
            }
            use core::fmt::Write;
            let _ = write!(msg, "{index}");
        }
        ctx.buffer.message(ctx.font, &msg);
    }
    ligate_input(
        ctx,
        components.len() + 1,
        match_end,
        total_component_count,
        ligature_glyph,
    )?;
    message!(ctx, "ligated glyph at {}", pos);
    Some(())
}

/// Reverse chaining single substitution.
pub(crate) struct ReverseChainSubst1<'a> {
    substitutes: Uints<'a>,
    /// The backtrack coverages, `None` where one can't be read.
    backtrack: CoverageArray<'a>,
    lookahead: CoverageArray<'a>,
}

impl<'a> ReverseChainSubst1<'a> {
    pub fn new(
        table: &ReverseChainSingleSubstFormat1<'a>,
        budget: &Budget,
    ) -> Result<Option<Self>, Exhausted> {
        let data = table.offset_data();
        let Some(substitutes) = Uints::new(data, table.substitute_glyph_ids_byte_range(), NARROW)
        else {
            return Ok(None);
        };
        Ok(Some(Self {
            substitutes,
            backtrack: CoverageArray::new(
                Offsets::new(data, table.backtrack_coverage_offsets_byte_range(), NARROW),
                budget,
            )?,
            lookahead: CoverageArray::new(
                Offsets::new(data, table.lookahead_coverage_offsets_byte_range(), NARROW),
                budget,
            )?,
        }))
    }

    pub fn new_wide(
        table: &ReverseChainSingleSubstFormat2<'a>,
        budget: &Budget,
    ) -> Result<Option<Self>, Exhausted> {
        let data = table.offset_data();
        let Some(substitutes) = Uints::new(data, table.substitute_glyph_ids_byte_range(), WIDE)
        else {
            return Ok(None);
        };
        Ok(Some(Self {
            substitutes,
            backtrack: CoverageArray::new(
                Offsets::new(data, table.backtrack_coverage_offsets_byte_range(), WIDE),
                budget,
            )?,
            lookahead: CoverageArray::new(
                Offsets::new(data, table.lookahead_coverage_offsets_byte_range(), WIDE),
                budget,
            )?,
        }))
    }

    pub fn apply(&self, ctx: &mut ApplyContext, index: u32) -> Option<()> {
        // Reverse chaining lookups can't be nested.
        if ctx.nesting_level_left != MAX_NESTING_LEVEL {
            return None;
        }
        let substitute = self.substitutes.get(index as usize)?;

        let mut start_index = 0;
        let mut end_index = 0;
        if match_backtrack(
            ctx,
            self.backtrack.len() as u16,
            |info, index| covered_at(&self.backtrack, index, info),
            &mut start_index,
        ) && match_lookahead(
            ctx,
            self.lookahead.len() as u16,
            |info, index| covered_at(&self.lookahead, index, info),
            ctx.buffer.idx + 1,
            &mut end_index,
        ) {
            message!(
                ctx,
                "replacing glyph at {} (reverse chaining substitution)",
                ctx.buffer.message_idx()
            );
            ctx.buffer
                .unsafe_to_break_from_outbuffer(Some(start_index), Some(end_index));
            if ctx.replace_glyph_inplace(substitute.into()).is_none() {
                ctx.buffer.index_failed();
            }
            message!(
                ctx,
                "replaced glyph at {} (reverse chaining substitution)",
                ctx.buffer.message_idx()
            );
            // The caller moves back to the previous glyph, which keeps a
            // call through a context lookup from skipping one.
            return Some(());
        }
        ctx.buffer
            .unsafe_to_concat_from_outbuffer(Some(start_index), Some(end_index));
        None
    }
}

impl<'a> SubtableDigests<'a> for ReverseChainSubst1<'a> {}

/// Whether coverage `index` of `coverages` exists and covers the glyph.
#[inline(always)]
pub(super) fn covered_at(coverages: &CoverageArray, index: u32, info: &GlyphInfo) -> bool {
    coverages.get(index as usize).covers(info.glyph_id)
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::vec;
    use read_fonts::FontRead;

    fn seconds_of(data: &[u8], budget: &Budget) -> Result<SetDigest, Exhausted> {
        let table = LigatureSubstFormat1::read(FontData::new(data)).unwrap();
        LigatureSubst1::new(&table, budget).map(|subst| subst.seconds)
    }

    #[test]
    fn collect_seconds_bails_on_too_many_ligature_sets() {
        let ligature_set_count = 17;
        let mut data = vec![0; 6 + ligature_set_count * 2];
        data[0..2].copy_from_slice(&1u16.to_be_bytes());
        data[4..6].copy_from_slice(&(ligature_set_count as u16).to_be_bytes());
        assert_eq!(
            seconds_of(&data, &Budget::new(16, 0)).unwrap_err(),
            Exhausted
        );
    }

    #[test]
    fn collect_seconds_bails_on_too_many_ligatures() {
        let ligature_count = 16;
        let mut data = vec![0; 10 + ligature_count * 2];
        data[0..2].copy_from_slice(&1u16.to_be_bytes());
        data[4..6].copy_from_slice(&1u16.to_be_bytes());
        data[6..8].copy_from_slice(&8u16.to_be_bytes());
        data[8..10].copy_from_slice(&(ligature_count as u16).to_be_bytes());
        assert_eq!(
            seconds_of(&data, &Budget::new(16, 0)).unwrap_err(),
            Exhausted
        );
    }
}
