//! The [MVAR (Metrics Variation)](https://docs.microsoft.com/en-us/typography/opentype/spec/mvar) table

use super::variations::{DeltaSetIndex, ItemVariationStore};
use types::F48Dot16;

/// Four-byte tags used to represent particular metric or other values.
pub mod tags {
    use font_types::Tag;

    /// Horizontal ascender.
    pub const HASC: Tag = Tag::new(b"hasc");
    /// Horizontal descender.
    pub const HDSC: Tag = Tag::new(b"hdsc");
    /// Horizontal line gap.
    pub const HLGP: Tag = Tag::new(b"hlgp");

    /// Horizontal clipping ascent.
    pub const HCLA: Tag = Tag::new(b"hcla");
    /// Horizontal clipping descent.
    pub const HCLD: Tag = Tag::new(b"hcld");

    /// Vertical ascender.
    pub const VASC: Tag = Tag::new(b"vasc");
    /// Vertical descender.
    pub const VDSC: Tag = Tag::new(b"vdsc");
    /// Vertical line gap.
    pub const VLGP: Tag = Tag::new(b"vlgp");

    /// Horizontal caret rise.
    pub const HCRS: Tag = Tag::new(b"hcrs");
    /// Horizontal caret run.
    pub const HCRN: Tag = Tag::new(b"hcrn");
    /// Horizontal caret offset.
    pub const HCOF: Tag = Tag::new(b"hcof");

    /// Vertical caret rise.
    pub const VCRS: Tag = Tag::new(b"vcrs");
    /// Vertical caret run.
    pub const VCRN: Tag = Tag::new(b"vcrn");
    /// Vertical caret offset.
    pub const VCOF: Tag = Tag::new(b"vcof");

    /// X-height.
    pub const XHGT: Tag = Tag::new(b"xhgt");
    /// Cap height.
    pub const CPHT: Tag = Tag::new(b"cpht");

    /// Subscript em x-offset.
    pub const SBXO: Tag = Tag::new(b"sbxo");
    /// Subscript em y-offset.
    pub const SBYO: Tag = Tag::new(b"sbyo");
    /// Subscript em x-size.
    pub const SBXS: Tag = Tag::new(b"sbxs");
    /// Subscript em y-size.
    pub const SBYS: Tag = Tag::new(b"sbys");

    /// Superscript em x-offset.
    pub const SPXO: Tag = Tag::new(b"spxo");
    /// Superscript em y-offset.
    pub const SPYO: Tag = Tag::new(b"spyo");
    /// Superscript em x-size.
    pub const SPXS: Tag = Tag::new(b"spxs");
    /// Superscript em y-size.
    pub const SPYS: Tag = Tag::new(b"spys");

    /// Strikeout size.
    pub const STRS: Tag = Tag::new(b"strs");
    /// Strikeout offset.
    pub const STRO: Tag = Tag::new(b"stro");

    /// Underline size.
    pub const UNDS: Tag = Tag::new(b"unds");
    /// Underline offset.
    pub const UNDO: Tag = Tag::new(b"undo");

    /// GaspRange\[0\]
    pub const GSP0: Tag = Tag::new(b"gsp0");
    /// GaspRange\[1\]
    pub const GSP1: Tag = Tag::new(b"gsp1");
    /// GaspRange\[2\]
    pub const GSP2: Tag = Tag::new(b"gsp2");
    /// GaspRange\[3\]
    pub const GSP3: Tag = Tag::new(b"gsp3");
    /// GaspRange\[4\]
    pub const GSP4: Tag = Tag::new(b"gsp4");
    /// GaspRange\[5\]
    pub const GSP5: Tag = Tag::new(b"gsp5");
    /// GaspRange\[6\]
    pub const GSP6: Tag = Tag::new(b"gsp6");
    /// GaspRange\[7\]
    pub const GSP7: Tag = Tag::new(b"gsp7");
    /// GaspRange\[8\]
    pub const GSP8: Tag = Tag::new(b"gsp8");
    /// GaspRange\[9\]
    pub const GSP9: Tag = Tag::new(b"gsp9");
}

include!("../../generated/generated_mvar.rs");

/// `MVAR` at one location, with the variation store resolved once.
///
/// Prefer this over [`Mvar::metric_delta`] when reading more than one metric;
/// that resolves the store on every lookup.
pub struct MvarInstance<'a> {
    records: &'a [ValueRecord],
    ivs: ItemVariationStore<'a>,
    coords: &'a [F2Dot14],
}

impl<'a> Mvar<'a> {
    /// Returns the table at a location.
    ///
    /// `None` at the default location, where every delta is zero, and for a
    /// font whose variation store cannot be read.
    pub fn at(&self, coords: &'a [F2Dot14]) -> Option<MvarInstance<'a>> {
        if coords.is_empty() {
            return None;
        }
        Some(MvarInstance {
            records: self.value_records(),
            ivs: self.item_variation_store()?.ok()?,
            coords,
        })
    }
}

impl MvarInstance<'_> {
    /// Returns the delta for a metric, or `None` for one the font does not
    /// vary.
    ///
    /// Tags are in the [`tags`] module. The delta is in design units, added
    /// to the value the metric's own table holds, and left unrounded so a
    /// caller can round it its own way.
    pub fn get(&self, tag: Tag) -> Option<F48Dot16> {
        let index = self
            .records
            .binary_search_by(|record| record.value_tag().cmp(&tag))
            .ok()?;
        let record = &self.records[index];
        self.ivs.compute_delta(
            DeltaSetIndex {
                outer: record.delta_set_outer_index(),
                inner: record.delta_set_inner_index(),
            },
            self.coords,
        )
    }
}

impl Mvar<'_> {
    /// Returns the metric delta for the specified tag and normalized
    /// variation coordinates. Possible tags are found in the [tags]
    /// module.
    ///
    /// `None` for a metric the font does not vary, and for one it states
    /// unreadably.
    pub fn metric_delta(&self, tag: Tag, coords: &[F2Dot14]) -> Option<Fixed> {
        use std::cmp::Ordering;
        let records = self.value_records();
        let mut lo = 0;
        let mut hi = records.len();
        while lo < hi {
            let i = (lo + hi) / 2;
            let record = &records[i];
            match tag.cmp(&record.value_tag()) {
                Ordering::Less => {
                    hi = i;
                }
                Ordering::Greater => {
                    lo = i + 1;
                }
                Ordering::Equal => {
                    let ivs = self.item_variation_store()?.ok()?;
                    return Some(Fixed::from_i32(
                        ivs.compute_delta(
                            DeltaSetIndex {
                                outer: record.delta_set_outer_index(),
                                inner: record.delta_set_inner_index(),
                            },
                            coords,
                        )?
                        .to_i32(),
                    ));
                }
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};
    use types::F2Dot14;

    /// Twelve axes, and an `MVAR` varying several metrics.
    const VAR: &[u8] = font_test_data::AMSTELVAR_AVAR2_A;

    fn far() -> [F2Dot14; 12] {
        [F2Dot14::from_f32(1.0); 12]
    }

    #[test]
    fn the_default_location_has_no_instance() {
        // Every delta is zero there, so there is nothing to resolve.
        let font = FontRef::new(VAR).unwrap();
        assert!(font.mvar().unwrap().at(&[]).is_none());
    }

    #[test]
    fn an_instance_agrees_with_the_single_metric_accessor() {
        let font = FontRef::new(VAR).unwrap();
        let mvar = font.mvar().unwrap();
        let coords = far();
        let instance = mvar.at(&coords).expect("a location away from the default");
        let mut varied = 0;
        for record in mvar.value_records() {
            let tag = record.value_tag();
            let exact = instance.get(tag).expect("a tag the table states");
            let rounded = mvar.metric_delta(tag, &coords).unwrap();
            assert_eq!(exact.to_i32(), rounded.to_i32(), "{tag}");
            varied += (exact != F48Dot16::ZERO) as u32;
        }
        assert!(varied > 0, "no metric moved at the far end of every axis");
    }

    #[test]
    fn a_tag_the_font_does_not_state_is_absent() {
        let font = FontRef::new(VAR).unwrap();
        let mvar = font.mvar().unwrap();
        let coords = far();
        let instance = mvar.at(&coords).unwrap();
        assert!(instance.get(Tag::new(b"zzzz")).is_none());
    }

    #[test]
    fn an_instance_keeps_a_fraction_the_single_accessor_drops() {
        // `metric_delta` reports whole design units; the instance reports
        // what the variation store computed.
        let font = FontRef::new(VAR).unwrap();
        let mvar = font.mvar().unwrap();
        let coords = [F2Dot14::from_f32(0.37); 12];
        let instance = mvar.at(&coords).unwrap();
        let fractional = mvar
            .value_records()
            .iter()
            .filter_map(|record| instance.get(record.value_tag()))
            .any(|delta| delta.to_bits() & 0xFFFF != 0);
        assert!(fractional, "no metric delta carried a fraction");
    }
}
