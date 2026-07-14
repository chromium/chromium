//! The gvar table

include!("../../generated/generated_gvar.rs");

use std::collections::HashMap;

use indexmap::IndexMap;

use crate::{
    collections::HasLen,
    tables::variations::{Deltas, TupleVariationStoreInputError},
    OffsetMarker,
};

use super::variations::{
    compute_shared_points, compute_tuple_variation_count, compute_tuple_variation_data_offset,
    PackedDeltas, PackedPointNumbers, Tuple, TupleVariationCount, TupleVariationHeader,
};

pub use super::variations::Tent;

pub mod iup;

/// Variation data for a single glyph, before it is compiled
#[derive(Clone, Debug)]
pub struct GlyphVariations {
    gid: GlyphId,
    variations: Vec<GlyphDeltas>,
}

/// Glyph deltas for one point in the design space.
pub type GlyphDeltas = Deltas<GlyphDelta>;

/// An error representing invalid input when building a gvar table
pub type GvarInputError = TupleVariationStoreInputError<GlyphId>;

/// A delta for a single value in a glyph.
///
/// This includes a flag indicating whether or not this delta is required (i.e
/// it cannot be interpolated from neighbouring deltas and coordinates).
/// This is only relevant for simple glyphs; interpolatable points may be
/// omitted in the final binary when doing so saves space.
/// See <https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#inferred-deltas-for-un-referenced-point-numbers>
/// for more information.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct GlyphDelta {
    pub x: i16,
    pub y: i16,
    /// This delta must be included, i.e. cannot be interpolated
    pub required: bool,
}

impl Gvar {
    /// Construct a gvar table from a vector of per-glyph variations and the
    /// axis count.
    ///
    /// Variations must be present for each glyph, but may be empty.
    /// For non-empty variations, the axis count must be equal to the provided
    /// axis count, as specified by the 'fvar' table.
    pub fn new(
        mut variations: Vec<GlyphVariations>,
        axis_count: u16,
    ) -> Result<Self, GvarInputError> {
        fn compute_shared_peak_tuples(glyphs: &[GlyphVariations]) -> Vec<Tuple> {
            const MAX_SHARED_TUPLES: usize = 4095;
            let mut peak_tuple_counts = IndexMap::new();
            for glyph in glyphs {
                glyph.count_peak_tuples(&mut peak_tuple_counts);
            }
            let mut to_share =
                peak_tuple_counts.into_iter().filter(|(_, n)| *n > 1).collect::<Vec<_>>();
            // prefer IndexMap::sort_by_key over HashMap::sort_unstable_by_key so the
            // order of the shared tuples with equal count doesn't change randomly
            // but is kept stable to ensure builds are deterministic.
            to_share.sort_by_key(|(_, n)| std::cmp::Reverse(*n));
            to_share.truncate(MAX_SHARED_TUPLES);
            to_share.into_iter().map(|(t, _)| t.to_owned()).collect()
        }

        for var in &variations {
            var.validate()?;
        }

        if let Some(bad_var) = variations
            .iter()
            .find(|var| var.axis_count().is_some() && var.axis_count().unwrap() != axis_count)
        {
            return Err(TupleVariationStoreInputError::UnexpectedAxisCount {
                index: bad_var.gid,
                expected: axis_count,
                actual: bad_var.axis_count().unwrap(),
            });
        }

        let shared = compute_shared_peak_tuples(&variations);
        let shared_idx_map = shared.iter().enumerate().map(|(i, x)| (x, i as u16)).collect();
        variations.sort_unstable_by_key(|g| g.gid);
        let glyphs = variations.into_iter().map(|raw_g| raw_g.build(&shared_idx_map)).collect();

        Ok(Gvar {
            axis_count,
            shared_tuples: SharedTuples::new(shared).into(),
            glyph_variation_data_offsets: glyphs,
        })
    }

    fn compute_flags(&self) -> GvarFlags {
        let max_offset = self
            .glyph_variation_data_offsets
            .iter()
            .fold(0, |acc, val| acc + val.length + val.length % 2);

        if max_offset / 2 <= (u16::MAX as u32) {
            GvarFlags::default()
        } else {
            GvarFlags::LONG_OFFSETS
        }
    }

    fn compute_glyph_count(&self) -> u16 {
        self.glyph_variation_data_offsets.len().try_into().unwrap()
    }

    fn compute_shared_tuples_offset(&self) -> u32 {
        const BASE_OFFSET: usize = MajorMinor::RAW_BYTE_LEN
            + u16::RAW_BYTE_LEN // axis count
            + u16::RAW_BYTE_LEN // shared tuples count
            + Offset32::RAW_BYTE_LEN
            + u16::RAW_BYTE_LEN + u16::RAW_BYTE_LEN // glyph count, flags
            + u32::RAW_BYTE_LEN; // glyph_variation_data_array_offset

        let bytes_per_offset = if self.compute_flags() == GvarFlags::LONG_OFFSETS {
            u32::RAW_BYTE_LEN
        } else {
            u16::RAW_BYTE_LEN
        };

        let offsets_len = (self.glyph_variation_data_offsets.len() + 1) * bytes_per_offset;

        (BASE_OFFSET + offsets_len).try_into().unwrap()
    }

    fn compute_data_array_offset(&self) -> u32 {
        let shared_tuples_len: u32 =
            (array_len(&self.shared_tuples) * self.axis_count as usize * 2).try_into().unwrap();
        self.compute_shared_tuples_offset() + shared_tuples_len
    }

    fn compile_variation_data(&self) -> GlyphDataWriter<'_> {
        GlyphDataWriter {
            long_offsets: self.compute_flags() == GvarFlags::LONG_OFFSETS,
            shared_tuples: &self.shared_tuples,
            data: &self.glyph_variation_data_offsets,
        }
    }
}

impl GlyphVariations {
    /// Construct a new set of variation deltas for a glyph.
    pub fn new(gid: GlyphId, variations: Vec<GlyphDeltas>) -> Self {
        Self { gid, variations }
    }

    /// called when we build gvar, so we only return errors in one place
    fn validate(&self) -> Result<(), GvarInputError> {
        let (axis_count, delta_len) = self
            .variations
            .first()
            .map(|var| (var.peak_tuple.len(), var.deltas.len()))
            .unwrap_or_default();
        for var in &self.variations {
            if var.peak_tuple.len() != axis_count {
                return Err(TupleVariationStoreInputError::InconsistentAxisCount(self.gid));
            }
            if let Some((start, end)) = var.intermediate_region.as_ref() {
                if start.len() != axis_count || end.len() != axis_count {
                    return Err(TupleVariationStoreInputError::InconsistentTupleLengths(self.gid));
                }
            }
            if var.deltas.len() != delta_len {
                return Err(TupleVariationStoreInputError::InconsistentDeltaLength(self.gid));
            }
        }
        Ok(())
    }

    /// Will be `None` if there are no variations for this glyph
    pub fn axis_count(&self) -> Option<u16> {
        self.variations.first().map(|var| var.peak_tuple.len())
    }

    fn count_peak_tuples<'a>(&'a self, counter: &mut IndexMap<&'a Tuple, usize>) {
        for tuple in &self.variations {
            *counter.entry(&tuple.peak_tuple).or_default() += 1;
        }
    }

    fn build(self, shared_tuple_map: &HashMap<&Tuple, u16>) -> GlyphVariationData {
        let shared_points = compute_shared_points(&self.variations);

        let (tuple_headers, tuple_data): (Vec<_>, Vec<_>) = self
            .variations
            .into_iter()
            .map(|tup| tup.build(shared_tuple_map, shared_points.as_ref()))
            .unzip();

        let mut temp = GlyphVariationData {
            tuple_variation_headers: tuple_headers,
            shared_point_numbers: shared_points,
            per_tuple_data: tuple_data,
            length: 0,
        };

        temp.length = temp.compute_size();
        temp
    }
}

impl GlyphDelta {
    /// Create a new delta value.
    pub fn new(x: i16, y: i16, required: bool) -> Self {
        Self { x, y, required }
    }

    /// Create a new delta value that must be encoded (cannot be interpolated)
    pub fn required(x: i16, y: i16) -> Self {
        Self::new(x, y, true)
    }

    /// Create a new delta value that may be omitted (can be interpolated)
    pub fn optional(x: i16, y: i16) -> Self {
        Self::new(x, y, false)
    }
}

impl GlyphDeltas {
    /// Create a new set of deltas.
    ///
    /// A None delta means do not explicitly encode, typically because IUP
    /// suggests it isn't required.
    pub fn new(tents: Vec<Tent>, deltas: Vec<GlyphDelta>) -> Self {
        let peak_tuple = Tuple::new(tents.iter().map(Tent::peak).collect());

        // File size optimisation: if all the intermediates can be derived from
        // the relevant peak values, don't serialize them.
        // https://github.com/fonttools/fonttools/blob/b467579c/Lib/fontTools/ttLib/tables/TupleVariation.py#L184-L193
        let intermediate_region = if tents.iter().any(Tent::requires_intermediate) {
            Some(tents.iter().map(Tent::bounds).unzip())
        } else {
            None
        };

        // at construction time we build both iup optimized & not versions
        // of ourselves, to determine what representation is most efficient;
        // the caller will look at the generated packed points to decide which
        // set should be shared.
        let best_point_packing = Self::pick_best_point_number_repr(&deltas);
        GlyphDeltas { peak_tuple, intermediate_region, deltas, best_point_packing }
    }

    // this is a type method just to expose it for testing, we call it before
    // we finish instantiating self.
    //
    // we do a lot of duplicate work here with creating & throwing away
    // buffers, and that can be improved at the cost of a bit more complexity
    // <https://github.com/googlefonts/fontations/issues/635>
    fn pick_best_point_number_repr(deltas: &[GlyphDelta]) -> PackedPointNumbers {
        if deltas.iter().all(|d| d.required) {
            return PackedPointNumbers::All;
        }

        let dense = Self::build_non_sparse_data(deltas);
        let sparse = Self::build_sparse_data(deltas);
        let dense_size = dense.compute_size();
        let sparse_size = sparse.compute_size();
        log::trace!("dense {dense_size}, sparse {sparse_size}");
        if sparse_size < dense_size {
            sparse.private_point_numbers.unwrap()
        } else {
            PackedPointNumbers::All
        }
    }

    fn build_non_sparse_data(deltas: &[GlyphDelta]) -> GlyphTupleVariationData {
        let (x_deltas, y_deltas) =
            deltas.iter().map(|delta| (delta.x as i32, delta.y as i32)).unzip();
        GlyphTupleVariationData {
            private_point_numbers: Some(PackedPointNumbers::All),
            x_deltas: PackedDeltas::new(x_deltas),
            y_deltas: PackedDeltas::new(y_deltas),
        }
    }

    fn build_sparse_data(deltas: &[GlyphDelta]) -> GlyphTupleVariationData {
        let (x_deltas, y_deltas) = deltas
            .iter()
            .filter_map(|delta| delta.required.then_some((delta.x as i32, delta.y as i32)))
            .unzip();
        let point_numbers = deltas
            .iter()
            .enumerate()
            .filter_map(|(i, delta)| delta.required.then_some(i as u16))
            .collect();
        GlyphTupleVariationData {
            private_point_numbers: Some(PackedPointNumbers::Some(point_numbers)),
            x_deltas: PackedDeltas::new(x_deltas),
            y_deltas: PackedDeltas::new(y_deltas),
        }
    }

    // shared points is just "whatever points, if any, are shared." We are
    // responsible for seeing if these are actually our points, in which case
    // we are using shared points.
    fn build(
        self,
        shared_tuple_map: &HashMap<&Tuple, u16>,
        shared_points: Option<&PackedPointNumbers>,
    ) -> (TupleVariationHeader, GlyphTupleVariationData) {
        let GlyphDeltas {
            peak_tuple,
            intermediate_region,
            deltas,
            best_point_packing: point_numbers,
        } = self;

        let (idx, peak_tuple) = match shared_tuple_map.get(&peak_tuple) {
            Some(idx) => (Some(*idx), None),
            None => (None, Some(peak_tuple)),
        };

        let has_private_points = Some(&point_numbers) != shared_points;
        let (x_deltas, y_deltas) = match &point_numbers {
            PackedPointNumbers::All => deltas.iter().map(|d| (d.x as i32, d.y as i32)).unzip(),
            PackedPointNumbers::Some(pts) => pts
                .iter()
                .map(|idx| {
                    let delta = deltas[*idx as usize];
                    (delta.x as i32, delta.y as i32)
                })
                .unzip(),
        };

        let data = GlyphTupleVariationData {
            private_point_numbers: has_private_points.then_some(point_numbers),
            x_deltas: PackedDeltas::new(x_deltas),
            y_deltas: PackedDeltas::new(y_deltas),
        };
        let data_size = data.compute_size();

        let header = TupleVariationHeader::new(
            data_size,
            idx,
            peak_tuple,
            intermediate_region,
            has_private_points,
        );

        (header, data)
    }
}

/// The serializable representation of a glyph's variation data
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct GlyphVariationData {
    tuple_variation_headers: Vec<TupleVariationHeader>,
    // optional; present if multiple variations have the same point numbers
    shared_point_numbers: Option<PackedPointNumbers>,
    per_tuple_data: Vec<GlyphTupleVariationData>,
    /// calculated length required to store this data
    ///
    /// we compute this once up front because we need to know it in a bunch
    /// of different places (u32 because offsets are max u32)
    length: u32,
}

/// The serializable representation of a single glyph tuple variation data
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct GlyphTupleVariationData {
    // this is possibly shared, if multiple are identical for a given glyph
    private_point_numbers: Option<PackedPointNumbers>,
    x_deltas: PackedDeltas,
    y_deltas: PackedDeltas,
}

impl GlyphTupleVariationData {
    fn compute_size(&self) -> u16 {
        self.private_point_numbers
            .as_ref()
            .map(PackedPointNumbers::compute_size)
            .unwrap_or_default()
            .checked_add(self.x_deltas.compute_size())
            .unwrap()
            .checked_add(self.y_deltas.compute_size())
            .unwrap()
    }
}

impl FontWrite for GlyphTupleVariationData {
    fn write_into(&self, writer: &mut TableWriter) {
        self.private_point_numbers.write_into(writer);
        self.x_deltas.write_into(writer);
        self.y_deltas.write_into(writer);
    }
}

struct GlyphDataWriter<'a> {
    long_offsets: bool,
    shared_tuples: &'a SharedTuples,
    data: &'a [GlyphVariationData],
}

impl FontWrite for GlyphDataWriter<'_> {
    fn write_into(&self, writer: &mut TableWriter) {
        if self.long_offsets {
            let mut last = 0u32;
            last.write_into(writer);

            // write all the offsets
            for glyph in self.data {
                last += glyph.compute_size();
                last.write_into(writer);
            }
        } else {
            // for short offsets we divide the real offset by two; this means
            // we will have to add padding if necessary
            let mut last = 0u16;
            last.write_into(writer);

            // write all the offsets
            for glyph in self.data {
                let size = glyph.compute_size();
                // ensure we're always rounding up to the next 2
                let short_size = (size / 2) + size % 2;
                last += short_size as u16;
                last.write_into(writer);
            }
        }
        // then write the shared tuples (should come here according to the spec)
        // https://learn.microsoft.com/en-us/typography/opentype/spec/gvar#shared-tuples-array
        // > The shared tuples array follows the GlyphVariationData offsets array at the
        // > end of the 'gvar' header.
        self.shared_tuples.write_into(writer);
        // then write the actual data
        for glyph in self.data {
            if !glyph.is_empty() {
                glyph.write_into(writer);
                if !self.long_offsets {
                    writer.pad_to_2byte_aligned();
                }
            }
        }
    }
}

impl GlyphVariationData {
    fn compute_tuple_variation_count(&self) -> TupleVariationCount {
        compute_tuple_variation_count(
            self.tuple_variation_headers.len(),
            self.shared_point_numbers.is_some(),
        )
    }

    fn is_empty(&self) -> bool {
        self.tuple_variation_headers.is_empty()
    }

    fn compute_data_offset(&self) -> u16 {
        compute_tuple_variation_data_offset(
            &self.tuple_variation_headers,
            TupleVariationCount::RAW_BYTE_LEN + u16::RAW_BYTE_LEN,
        )
    }

    fn compute_size(&self) -> u32 {
        if self.is_empty() {
            return 0;
        }

        let data_start = self.compute_data_offset() as u32;
        let shared_point_len =
            self.shared_point_numbers.as_ref().map(|pts| pts.compute_size()).unwrap_or_default()
                as u32;
        let tuple_data_len =
            self.per_tuple_data.iter().fold(0u32, |acc, tup| acc + tup.compute_size() as u32);
        data_start + shared_point_len + tuple_data_len
    }
}

impl Extend<F2Dot14> for Tuple {
    fn extend<T: IntoIterator<Item = F2Dot14>>(&mut self, iter: T) {
        self.values.extend(iter);
    }
}

impl Validate for GlyphVariationData {
    fn validate_impl(&self, ctx: &mut ValidationCtx) {
        const MAX_TUPLE_VARIATIONS: usize = 4095;
        if !(0..=MAX_TUPLE_VARIATIONS).contains(&self.tuple_variation_headers.len()) {
            ctx.in_field("tuple_variation_headers", |ctx| {
                ctx.report("expected 0-4095 tuple variation tables")
            })
        }
    }
}

impl FontWrite for GlyphVariationData {
    fn write_into(&self, writer: &mut TableWriter) {
        self.compute_tuple_variation_count().write_into(writer);
        self.compute_data_offset().write_into(writer);
        self.tuple_variation_headers.write_into(writer);
        self.shared_point_numbers.write_into(writer);
        self.per_tuple_data.write_into(writer);
    }
}

impl HasLen for SharedTuples {
    fn len(&self) -> usize {
        self.tuples.len()
    }
}

impl FontWrite for TupleVariationCount {
    fn write_into(&self, writer: &mut TableWriter) {
        self.bits().write_into(writer)
    }
}
#[cfg(test)]
mod tests {
    use super::*;

    /// Helper function to concisely state test cases without intermediates.
    fn peaks(peaks: Vec<F2Dot14>) -> Vec<Tent> {
        peaks.into_iter().map(|peak| Tent::new(peak, None)).collect()
    }

    #[test]
    fn gvar_smoke_test() {
        let _ = env_logger::builder().is_test(true).try_init();
        let table = Gvar::new(
            vec![
                GlyphVariations::new(GlyphId::new(0), vec![]),
                GlyphVariations::new(
                    GlyphId::new(1),
                    vec![GlyphDeltas::new(
                        peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                        vec![
                            GlyphDelta::required(30, 31),
                            GlyphDelta::required(40, 41),
                            GlyphDelta::required(-50, -49),
                            GlyphDelta::required(101, 102),
                            GlyphDelta::required(10, 11),
                        ],
                    )],
                ),
                GlyphVariations::new(
                    GlyphId::new(2),
                    vec![
                        GlyphDeltas::new(
                            peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                            vec![
                                GlyphDelta::required(11, -20),
                                GlyphDelta::required(69, -41),
                                GlyphDelta::required(-69, 49),
                                GlyphDelta::required(168, 101),
                                GlyphDelta::required(1, 2),
                            ],
                        ),
                        GlyphDeltas::new(
                            peaks(vec![F2Dot14::from_f32(0.8), F2Dot14::from_f32(1.0)]),
                            vec![
                                GlyphDelta::required(3, -200),
                                GlyphDelta::required(4, -500),
                                GlyphDelta::required(5, -800),
                                GlyphDelta::required(6, -1200),
                                GlyphDelta::required(7, -1500),
                            ],
                        ),
                    ],
                ),
            ],
            2,
        )
        .unwrap();

        let g2 = &table.glyph_variation_data_offsets[1];
        let computed = g2.compute_size();
        let actual = crate::dump_table(g2).unwrap().len();
        assert_eq!(computed as usize, actual);

        let bytes = crate::dump_table(&table).unwrap();
        let gvar = read_fonts::tables::gvar::Gvar::read(FontData::new(&bytes)).unwrap();
        assert_eq!(gvar.version(), MajorMinor::VERSION_1_0);
        assert_eq!(gvar.shared_tuple_count(), 1);
        assert_eq!(gvar.glyph_count(), 3);
        // Check offsets, the shared_tuples_offset should point to just after the
        // table's header
        assert_eq!(gvar.shared_tuples_offset(), Offset32::new(28));
        assert_eq!(gvar.glyph_variation_data_array_offset(), 32);

        let g1 = gvar.glyph_variation_data(GlyphId::new(1)).unwrap().unwrap();
        let g1tup = g1.tuples().collect::<Vec<_>>();
        assert_eq!(g1tup.len(), 1);

        let (x, y): (Vec<_>, Vec<_>) = g1tup[0].deltas().map(|d| (d.x_delta, d.y_delta)).unzip();
        assert_eq!(x, vec![30, 40, -50, 101, 10]);
        assert_eq!(y, vec![31, 41, -49, 102, 11]);

        let g2 = gvar.glyph_variation_data(GlyphId::new(2)).unwrap().unwrap();
        let g2tup = g2.tuples().collect::<Vec<_>>();
        assert_eq!(g2tup.len(), 2);

        let (x, y): (Vec<_>, Vec<_>) = g2tup[0].deltas().map(|d| (d.x_delta, d.y_delta)).unzip();
        assert_eq!(x, vec![11, 69, -69, 168, 1]);
        assert_eq!(y, vec![-20, -41, 49, 101, 2]);

        let (x, y): (Vec<_>, Vec<_>) = g2tup[1].deltas().map(|d| (d.x_delta, d.y_delta)).unzip();

        assert_eq!(x, vec![3, 4, 5, 6, 7]);
        assert_eq!(y, vec![-200, -500, -800, -1200, -1500]);
    }

    #[test]
    fn use_iup_when_appropriate() {
        // IFF iup provides space savings, we should prefer it.
        let _ = env_logger::builder().is_test(true).try_init();
        let gid = GlyphId::new(0);
        let table = Gvar::new(
            vec![GlyphVariations::new(
                gid,
                vec![GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::required(30, 31),
                        GlyphDelta::optional(30, 31),
                        GlyphDelta::optional(30, 31),
                        GlyphDelta::required(101, 102),
                        GlyphDelta::required(10, 11),
                        GlyphDelta::optional(10, 11),
                    ],
                )],
            )],
            2,
        )
        .unwrap();

        let bytes = crate::dump_table(&table).unwrap();
        let gvar = read_fonts::tables::gvar::Gvar::read(FontData::new(&bytes)).unwrap();
        assert_eq!(gvar.version(), MajorMinor::VERSION_1_0);
        assert_eq!(gvar.shared_tuple_count(), 0);
        assert_eq!(gvar.glyph_count(), 1);

        let g1 = gvar.glyph_variation_data(gid).unwrap().unwrap();
        let g1tup = g1.tuples().collect::<Vec<_>>();
        assert_eq!(g1tup.len(), 1);
        let tuple_variation = &g1tup[0];

        assert!(!tuple_variation.has_deltas_for_all_points());
        assert_eq!(vec![0, 3, 4], tuple_variation.point_numbers().collect::<Vec<_>>());

        let points: Vec<_> = tuple_variation.deltas().map(|d| (d.x_delta, d.y_delta)).collect();
        assert_eq!(points, vec![(30, 31), (101, 102), (10, 11)]);
    }

    #[test]
    fn disregard_iup_when_appropriate() {
        // if the cost of encoding the list of points is greater than the savings
        // from omitting some deltas, we should just encode explicit zeros
        let points = vec![
            GlyphDelta::required(1, 2),
            GlyphDelta::required(3, 4),
            GlyphDelta::required(5, 6),
            GlyphDelta::optional(5, 6),
            GlyphDelta::required(7, 8),
        ];
        let gid = GlyphId::new(0);
        let table = Gvar::new(
            vec![GlyphVariations::new(
                gid,
                vec![GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    points,
                )],
            )],
            2,
        )
        .unwrap();
        let bytes = crate::dump_table(&table).unwrap();
        let gvar = read_fonts::tables::gvar::Gvar::read(FontData::new(&bytes)).unwrap();
        assert_eq!(gvar.version(), MajorMinor::VERSION_1_0);
        assert_eq!(gvar.shared_tuple_count(), 0);
        assert_eq!(gvar.glyph_count(), 1);

        let g1 = gvar.glyph_variation_data(gid).unwrap().unwrap();
        let g1tup = g1.tuples().collect::<Vec<_>>();
        assert_eq!(g1tup.len(), 1);
        let tuple_variation = &g1tup[0];

        assert!(tuple_variation.has_deltas_for_all_points());
        let points: Vec<_> = tuple_variation.deltas().map(|d| (d.x_delta, d.y_delta)).collect();
        assert_eq!(points, vec![(1, 2), (3, 4), (5, 6), (5, 6), (7, 8)]);
    }

    #[test]
    fn share_points() {
        let _ = env_logger::builder().is_test(true).try_init();
        let variations = GlyphVariations::new(
            GlyphId::new(0),
            vec![
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::required(1, 2),
                        GlyphDelta::optional(3, 4),
                        GlyphDelta::required(5, 6),
                        GlyphDelta::optional(5, 6),
                        GlyphDelta::required(7, 8),
                        GlyphDelta::optional(7, 8),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(-1.0), F2Dot14::from_f32(-1.0)]),
                    vec![
                        GlyphDelta::required(10, 20),
                        GlyphDelta::optional(30, 40),
                        GlyphDelta::required(50, 60),
                        GlyphDelta::optional(50, 60),
                        GlyphDelta::required(70, 80),
                        GlyphDelta::optional(70, 80),
                    ],
                ),
            ],
        );

        assert_eq!(
            compute_shared_points(&variations.variations),
            Some(PackedPointNumbers::Some(vec![0, 2, 4]))
        )
    }

    #[test]
    fn share_all_points() {
        let variations = GlyphVariations::new(
            GlyphId::new(0),
            vec![
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::required(1, 2),
                        GlyphDelta::required(3, 4),
                        GlyphDelta::required(5, 6),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(-1.0), F2Dot14::from_f32(-1.0)]),
                    vec![
                        GlyphDelta::required(2, 4),
                        GlyphDelta::required(6, 8),
                        GlyphDelta::required(7, 9),
                    ],
                ),
            ],
        );

        let shared_tups = HashMap::new();
        let built = variations.build(&shared_tups);
        assert_eq!(built.shared_point_numbers, Some(PackedPointNumbers::All))
    }

    // three tuples with three different packedpoint representations means
    // that we should have no shared points
    #[test]
    fn dont_share_unique_points() {
        let variations = GlyphVariations::new(
            GlyphId::new(0),
            vec![
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::required(1, 2),
                        GlyphDelta::optional(3, 4),
                        GlyphDelta::required(5, 6),
                        GlyphDelta::optional(5, 6),
                        GlyphDelta::required(7, 8),
                        GlyphDelta::optional(7, 8),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(-1.0), F2Dot14::from_f32(-1.0)]),
                    vec![
                        GlyphDelta::required(10, 20),
                        GlyphDelta::required(35, 40),
                        GlyphDelta::required(50, 60),
                        GlyphDelta::optional(50, 60),
                        GlyphDelta::required(70, 80),
                        GlyphDelta::optional(70, 80),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(0.5), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::required(1, 2),
                        GlyphDelta::optional(3, 4),
                        GlyphDelta::required(5, 6),
                        GlyphDelta::optional(5, 6),
                        GlyphDelta::optional(7, 8),
                        GlyphDelta::optional(7, 8),
                    ],
                ),
            ],
        );

        let shared_tups = HashMap::new();
        let built = variations.build(&shared_tups);
        assert!(built.shared_point_numbers.is_none());
    }

    // comparing our behaviour against what we know fonttools does.
    #[test]
    #[allow(non_snake_case)]
    fn oswald_Lcaron() {
        let _ = env_logger::builder().is_test(true).try_init();
        // in this glyph, it is more efficient to encode all points for the first
        // tuple, but sparse points for the second (the single y delta in the
        // second tuple means you can't encode the y-deltas as 'all zero')
        let variations = GlyphVariations::new(
            GlyphId::new(0),
            vec![
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(-1.0), F2Dot14::from_f32(-1.0)]),
                    vec![
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(35, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(-24, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                    vec![
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(26, 15),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(46, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
            ],
        );
        assert!(compute_shared_points(&variations.variations).is_none());
        let tups = HashMap::new();
        let built = variations.build(&tups);
        assert_eq!(built.per_tuple_data[0].private_point_numbers, Some(PackedPointNumbers::All));
        assert_eq!(
            built.per_tuple_data[1].private_point_numbers,
            Some(PackedPointNumbers::Some(vec![1, 3]))
        );
    }

    #[test]
    fn compute_shared_points_is_deterministic() {
        // The deltas for glyph "etatonos.sc.ss06" in GoogleSans-VF are such that the
        // TupleVariationStore's shared set of point numbers could potentionally be
        // computed as either PackedPointNumbers::All or PackedPointNumbers::Some([1,
        // 3]) without affecting the size (or correctness) of the serialized
        // data. However we want to ensure that the result is deterministic, and
        // doesn't depend on e.g. HashMap random iteration order.
        // https://github.com/googlefonts/fontc/issues/647
        let _ = env_logger::builder().is_test(true).try_init();
        let variations = GlyphVariations::new(
            GlyphId::NOTDEF,
            vec![
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(-1.0),
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(0.0),
                    ]),
                    vec![
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(-17, -4),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(-28, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(1.0),
                        F2Dot14::from_f32(0.0),
                    ]),
                    vec![
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(0, -10),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(34, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(-1.0),
                    ]),
                    vec![
                        GlyphDelta::required(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(1.0),
                    ]),
                    vec![
                        GlyphDelta::required(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(-1.0),
                        F2Dot14::from_f32(1.0),
                        F2Dot14::from_f32(0.0),
                    ]),
                    vec![
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(-1, 10),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::required(-9, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(-1.0),
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(-1.0),
                    ]),
                    vec![
                        GlyphDelta::required(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
                GlyphDeltas::new(
                    peaks(vec![
                        F2Dot14::from_f32(-1.0),
                        F2Dot14::from_f32(0.0),
                        F2Dot14::from_f32(1.0),
                    ]),
                    vec![
                        GlyphDelta::required(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                        GlyphDelta::optional(0, 0),
                    ],
                ),
            ],
        );

        assert_eq!(
            compute_shared_points(&variations.variations),
            // Also PackedPointNumbers::All would work, but Some([1, 3]) happens
            // to be the first one that fits the bill when iterating over the
            // tuple variations in the order they are listed for this glyph.
            Some(PackedPointNumbers::Some(vec![1, 3]))
        );
    }

    // when using short offsets we store (real offset / 2), so all offsets must
    // be even, which means when we have an odd number of bytes we have to pad.
    fn make_31_bytes_of_variation_data() -> Vec<GlyphDeltas> {
        vec![
            GlyphDeltas::new(
                peaks(vec![F2Dot14::from_f32(-1.0), F2Dot14::from_f32(-1.0)]),
                vec![
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::required(35, 0),
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::required(-24, 0),
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::optional(0, 0),
                ],
            ),
            GlyphDeltas::new(
                peaks(vec![F2Dot14::from_f32(1.0), F2Dot14::from_f32(1.0)]),
                vec![
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::required(26, 15),
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::required(46, 0),
                    GlyphDelta::optional(0, 0),
                    GlyphDelta::required(1, 0),
                ],
            ),
        ]
    }

    // sanity checking my input data for two subsequent tests
    #[test]
    fn who_tests_the_testers() {
        let variations = GlyphVariations::new(GlyphId::NOTDEF, make_31_bytes_of_variation_data());
        let mut tupl_map = HashMap::new();

        // without shared tuples this would be 39 bytes
        assert_eq!(variations.clone().build(&tupl_map).length, 39);

        // to get our real size we need to mock up the shared tuples:
        tupl_map.insert(&variations.variations[0].peak_tuple, 1);
        tupl_map.insert(&variations.variations[1].peak_tuple, 2);

        let built = variations.clone().build(&tupl_map);
        // we need an odd number to test impact of padding
        assert_eq!(built.length, 31);
    }

    fn assert_test_offset_packing(n_glyphs: u16, should_be_short: bool) {
        let (offset_len, data_len, expected_flags) = if should_be_short {
            // when using short offset we need to pad data to ensure offset is even
            (u16::RAW_BYTE_LEN, 32, GvarFlags::empty())
        } else {
            (u32::RAW_BYTE_LEN, 31, GvarFlags::LONG_OFFSETS)
        };

        let test_data = make_31_bytes_of_variation_data();
        let a_small_number_of_variations = (0..n_glyphs)
            .map(|i| GlyphVariations::new(GlyphId::from(i), test_data.clone()))
            .collect();

        let gvar = Gvar::new(a_small_number_of_variations, 2).unwrap();
        assert_eq!(gvar.compute_flags(), expected_flags);

        let writer = gvar.compile_variation_data();
        let mut sink = TableWriter::default();
        writer.write_into(&mut sink);

        let bytes = sink.into_data().bytes;
        let expected_len = (n_glyphs + 1) as usize * offset_len // offsets
                             + 8 // shared tuples TODO remove magic number
                             + data_len * n_glyphs as usize; // rounded size of each glyph
        assert_eq!(bytes.len(), expected_len);

        let dumped = crate::dump_table(&gvar).unwrap();
        let loaded = read_fonts::tables::gvar::Gvar::read(FontData::new(&dumped)).unwrap();

        assert_eq!(loaded.glyph_count(), n_glyphs);
        assert_eq!(loaded.flags(), expected_flags);
        assert!(loaded
            .glyph_variation_data_offsets()
            .iter()
            .map(|off| off.unwrap().get())
            .enumerate()
            .all(|(i, off)| off as usize == i * data_len));
    }

    #[test]
    fn prefer_short_offsets() {
        let _ = env_logger::builder().is_test(true).try_init();
        assert_test_offset_packing(5, true);
    }

    #[test]
    fn use_long_offsets_when_necessary() {
        // 2**16 * 2 / (31 + 1 padding) (bytes per tuple) = 4096 should be the first
        // overflow
        let _ = env_logger::builder().is_test(true).try_init();
        assert_test_offset_packing(4095, true);
        assert_test_offset_packing(4096, false);
        assert_test_offset_packing(4097, false);
    }

    #[test]
    fn shared_tuples_stable_order() {
        // Test that shared tuples are sorted stably and builds reproducible
        // https://github.com/googlefonts/fontc/issues/647
        let mut variations = Vec::new();
        for i in 0..2 {
            variations.push(GlyphVariations::new(
                GlyphId::new(i),
                vec![
                    GlyphDeltas::new(
                        peaks(vec![F2Dot14::from_f32(1.0)]),
                        vec![GlyphDelta::required(10, 20)],
                    ),
                    GlyphDeltas::new(
                        peaks(vec![F2Dot14::from_f32(-1.0)]),
                        vec![GlyphDelta::required(-10, -20)],
                    ),
                ],
            ))
        }
        for _ in 0..10 {
            let table = Gvar::new(variations.clone(), 1).unwrap();
            let bytes = crate::dump_table(&table).unwrap();
            let gvar = read_fonts::tables::gvar::Gvar::read(FontData::new(&bytes)).unwrap();

            assert_eq!(gvar.shared_tuple_count(), 2);
            assert_eq!(
                gvar.shared_tuples()
                    .unwrap()
                    .tuples()
                    .iter()
                    .map(|t| t.unwrap().values.to_vec())
                    .collect::<Vec<_>>(),
                vec![vec![F2Dot14::from_f32(1.0)], vec![F2Dot14::from_f32(-1.0)]]
            );
        }
    }

    #[test]
    fn unexpected_axis_count() {
        let variations = GlyphVariations::new(
            GlyphId::NOTDEF,
            vec![
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0)]),
                    vec![GlyphDelta::required(1, 2)],
                ),
                GlyphDeltas::new(
                    peaks(vec![F2Dot14::from_f32(1.0)]),
                    vec![GlyphDelta::required(1, 2)],
                ),
            ],
        );
        let gvar = Gvar::new(vec![variations], 2);
        assert!(matches!(
            gvar,
            Err(TupleVariationStoreInputError::UnexpectedAxisCount {
                index: GlyphId::NOTDEF,
                expected: 2,
                actual: 1
            })
        ));
    }

    #[test]
    fn empty_gvar_has_expected_axis_count() {
        let variations = GlyphVariations::new(GlyphId::NOTDEF, vec![]);
        let gvar = Gvar::new(vec![variations], 2).unwrap();
        assert_eq!(gvar.axis_count, 2);
    }

    #[test]
    /// Test the logic for determining whether individual intermediates need to
    /// be serialised in the context of their peak coordinates.
    fn intermediates_only_when_explicit_needed() {
        let any_points = vec![]; // could be anything

        // If an intermediate is not provided, one SHOULD NOT be serialised.
        let deltas =
            GlyphDeltas::new(vec![Tent::new(F2Dot14::from_f32(0.5), None)], any_points.clone());
        assert_eq!(deltas.intermediate_region, None);

        // If an intermediate is provided but is equal to the implicit
        // intermediate from the peak, it SHOULD NOT be serialised.
        let deltas = GlyphDeltas::new(
            vec![Tent::new(
                F2Dot14::from_f32(0.5),
                Some(Tent::implied_intermediates_for_peak(F2Dot14::from_f32(0.5))),
            )],
            any_points.clone(),
        );
        assert_eq!(deltas.intermediate_region, None);

        // If an intermediate is provided and it is not equal to the implicit
        // intermediate from the peak, it SHOULD be serialised.
        let deltas = GlyphDeltas::new(
            vec![Tent::new(
                F2Dot14::from_f32(0.5),
                Some((F2Dot14::from_f32(-0.3), F2Dot14::from_f32(0.4))),
            )],
            any_points.clone(),
        );
        assert_eq!(
            deltas.intermediate_region,
            Some((
                Tuple::new(vec![F2Dot14::from_f32(-0.3)]),
                Tuple::new(vec![F2Dot14::from_f32(0.4)]),
            ))
        );
    }

    #[test]
    /// Test the logic for determining whether multiple intermediates need to be
    /// serialised in the context of their peak coordinates and each other.
    fn intermediates_only_when_at_least_one_needed() {
        let any_points = vec![]; // could be anything

        // If every intermediate can be implied, none should be serialised.
        let deltas = GlyphDeltas::new(
            vec![Tent::new(F2Dot14::from_f32(0.5), None), Tent::new(F2Dot14::from_f32(0.5), None)],
            any_points.clone(),
        );
        assert_eq!(deltas.intermediate_region, None);

        // If even one intermediate cannot be implied, all should be serialised.
        let deltas = GlyphDeltas::new(
            vec![
                Tent::new(F2Dot14::from_f32(0.5), None),
                Tent::new(F2Dot14::from_f32(0.5), None),
                Tent::new(
                    F2Dot14::from_f32(0.5),
                    Some((F2Dot14::from_f32(-0.3), F2Dot14::from_f32(0.4))),
                ),
            ],
            any_points,
        );
        assert_eq!(
            deltas.intermediate_region,
            Some((
                Tuple::new(vec![
                    F2Dot14::from_f32(0.0),
                    F2Dot14::from_f32(0.0),
                    F2Dot14::from_f32(-0.3)
                ]),
                Tuple::new(vec![
                    F2Dot14::from_f32(0.5),
                    F2Dot14::from_f32(0.5),
                    F2Dot14::from_f32(0.4)
                ]),
            ))
        );
    }
}
