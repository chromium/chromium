//! The [cvar](https://learn.microsoft.com/en-us/typography/opentype/spec/cvar) table.
//!
//! This table uses a tuple variation store very similar to per-glyph variation
//! data in `gvar`, but with one set of scalar deltas (CVT entries) instead of
//! paired x/y deltas.

use read_fonts::TopLevelTable;
use types::{MajorMinor, Offset16, Tag};

include!("../../generated/generated_cvar.rs");

use crate::{
    table_type::TableType,
    tables::variations::{Deltas, TupleVariationStoreInputError},
    types::FixedSize,
    validate::{Validate, ValidationCtx},
    FontWrite, TableWriter,
};

use super::variations::{
    compute_shared_points, compute_tuple_variation_count, compute_tuple_variation_data_offset,
    PackedDeltas, PackedPointNumbers, Tent, Tuple, TupleVariationCount, TupleVariationHeader,
};

/// Delta values for one region in design space.
pub type CvtDeltas = Deltas<i32>;

#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct CvarVariationData {
    tuple_variation_headers: Vec<TupleVariationHeader>,
    shared_point_numbers: Option<PackedPointNumbers>,
    per_tuple_data: Vec<CvarTupleVariationData>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
struct CvarTupleVariationData {
    private_point_numbers: Option<PackedPointNumbers>,
    deltas: PackedDeltas,
}

impl Cvar {
    /// Construct a `cvar` table from a set of tuple deltas and expected axis count.
    pub fn new(
        variations: Vec<CvtDeltas>,
        axis_count: u16,
    ) -> Result<Self, TupleVariationStoreInputError<usize>> {
        if let Some(first) = variations.first() {
            let found_axis_count = first.peak_tuple.len();
            first.validate_against(axis_count)?;
            let expected_delta_len = first.deltas.len();
            for (index, var) in variations[1..].iter().enumerate() {
                if var.peak_tuple.len() != found_axis_count {
                    return Err(TupleVariationStoreInputError::InconsistentAxisCount(index));
                }
                var.validate_against(axis_count)?;
                if var.deltas.len() != expected_delta_len {
                    return Err(TupleVariationStoreInputError::InconsistentDeltaLength(
                        index,
                    ));
                }
            }
        }

        let tuples = CvarVariationData::from_variations(variations);
        Ok(Self {
            tuple_variation_headers: tuples,
        })
    }
}

impl CvtDeltas {
    /// Create a new set of CVT deltas for a tuple variation.
    pub fn new(tents: Vec<Tent>, deltas: Vec<i32>) -> Self {
        let peak_tuple = Tuple::new(tents.iter().map(Tent::peak).collect());
        let intermediate_region = if tents.iter().any(Tent::requires_intermediate) {
            Some(tents.iter().map(Tent::bounds).unzip())
        } else {
            None
        };

        let best_point_packing = Self::pick_best_point_number_repr(&deltas);
        Self {
            peak_tuple,
            intermediate_region,
            deltas,
            best_point_packing,
        }
    }

    fn validate_against(
        &self,
        axis_count: u16,
    ) -> Result<(), TupleVariationStoreInputError<usize>> {
        if self.peak_tuple.len() != axis_count {
            return Err(TupleVariationStoreInputError::UnexpectedAxisCount {
                expected: axis_count,
                actual: self.peak_tuple.len(),
                index: 0,
            });
        }

        if let Some((start, end)) = self.intermediate_region.as_ref() {
            if start.len() != axis_count || end.len() != axis_count {
                return Err(TupleVariationStoreInputError::InconsistentTupleLengths(0));
            }
        }
        Ok(())
    }

    fn pick_best_point_number_repr(deltas: &[i32]) -> PackedPointNumbers {
        if deltas.iter().all(|d| *d != 0) {
            return PackedPointNumbers::All;
        }

        let dense = Self::build_non_sparse_data(deltas);
        let sparse = Self::build_sparse_data(deltas);

        if sparse.compute_size() < dense.compute_size() {
            sparse.private_point_numbers.unwrap()
        } else {
            PackedPointNumbers::All
        }
    }

    fn build_non_sparse_data(deltas: &[i32]) -> CvarTupleVariationData {
        CvarTupleVariationData {
            private_point_numbers: Some(PackedPointNumbers::All),
            deltas: PackedDeltas::new(deltas.to_vec()),
        }
    }

    fn build_sparse_data(deltas: &[i32]) -> CvarTupleVariationData {
        let sparse_deltas = deltas.iter().copied().filter(|delta| *delta != 0).collect();
        let point_numbers = deltas
            .iter()
            .enumerate()
            .filter_map(|(i, delta)| (*delta != 0).then_some(i as u16))
            .collect();

        CvarTupleVariationData {
            private_point_numbers: Some(PackedPointNumbers::Some(point_numbers)),
            deltas: PackedDeltas::new(sparse_deltas),
        }
    }

    fn build(
        self,
        shared_points: Option<&PackedPointNumbers>,
    ) -> (TupleVariationHeader, CvarTupleVariationData) {
        let CvtDeltas {
            peak_tuple,
            intermediate_region,
            deltas,
            best_point_packing: point_numbers,
        } = self;

        let has_private_points = Some(&point_numbers) != shared_points;
        let packed_deltas = match &point_numbers {
            PackedPointNumbers::All => deltas,
            PackedPointNumbers::Some(pts) => pts.iter().map(|idx| deltas[*idx as usize]).collect(),
        };

        let data = CvarTupleVariationData {
            private_point_numbers: has_private_points.then_some(point_numbers),
            deltas: PackedDeltas::new(packed_deltas),
        };

        let header = TupleVariationHeader::new(
            data.compute_size(),
            None,
            Some(peak_tuple),
            intermediate_region,
            has_private_points,
        );

        (header, data)
    }
}

impl CvarVariationData {
    fn from_variations(variations: Vec<CvtDeltas>) -> Self {
        let shared_points = compute_shared_points(&variations);
        let (tuple_variation_headers, per_tuple_data): (Vec<_>, Vec<_>) = variations
            .into_iter()
            .map(|var| var.build(shared_points.as_ref()))
            .unzip();

        Self {
            tuple_variation_headers,
            shared_point_numbers: shared_points,
            per_tuple_data,
        }
    }

    fn compute_tuple_variation_count(&self) -> TupleVariationCount {
        compute_tuple_variation_count(
            self.tuple_variation_headers.len(),
            self.shared_point_numbers.is_some(),
        )
    }

    fn compute_data_offset(&self) -> u16 {
        compute_tuple_variation_data_offset(
            &self.tuple_variation_headers,
            MajorMinor::RAW_BYTE_LEN + TupleVariationCount::RAW_BYTE_LEN + Offset16::RAW_BYTE_LEN,
        )
    }
}

impl Validate for CvarVariationData {
    fn validate_impl(&self, ctx: &mut ValidationCtx) {
        const MAX_TUPLE_VARIATIONS: usize = 4095;
        if !(0..=MAX_TUPLE_VARIATIONS).contains(&self.tuple_variation_headers.len()) {
            ctx.in_field("tuple_variation_headers", |ctx| {
                ctx.report("expected 0-4095 tuple variation tables")
            })
        }
    }
}

impl FontWrite for CvarVariationData {
    fn write_into(&self, writer: &mut TableWriter) {
        self.compute_tuple_variation_count().write_into(writer);
        self.compute_data_offset().write_into(writer);
        self.tuple_variation_headers.write_into(writer);
        self.shared_point_numbers.write_into(writer);
        self.per_tuple_data.write_into(writer);
    }
}

impl CvarTupleVariationData {
    fn compute_size(&self) -> u16 {
        self.private_point_numbers
            .as_ref()
            .map(PackedPointNumbers::compute_size)
            .unwrap_or_default()
            .checked_add(self.deltas.compute_size())
            .unwrap()
    }
}

impl FontWrite for CvarTupleVariationData {
    fn write_into(&self, writer: &mut TableWriter) {
        self.private_point_numbers.write_into(writer);
        self.deltas.write_into(writer);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use read_fonts::{FontData, FontRead};
    use types::F2Dot14;

    fn peaks(peaks: Vec<F2Dot14>) -> Vec<Tent> {
        peaks
            .into_iter()
            .map(|peak| Tent::new(peak, None))
            .collect()
    }

    #[test]
    fn cvar_smoke_test() {
        let table = Cvar::new(
            vec![
                CvtDeltas::new(peaks(vec![F2Dot14::from_f32(1.0)]), vec![10, 20, 0, -5, 0]),
                CvtDeltas::new(peaks(vec![F2Dot14::from_f32(-1.0)]), vec![0, -6, 8, 0, 0]),
            ],
            1,
        )
        .unwrap();

        let bytes = crate::dump_table(&table).unwrap();
        let read = read_fonts::tables::cvar::Cvar::read(FontData::new(&bytes)).unwrap();
        assert_eq!(read.version(), MajorMinor::VERSION_1_0);

        let var_data = read.variation_data(1).unwrap();
        let tuples = var_data.tuples().collect::<Vec<_>>();
        assert_eq!(tuples.len(), 2);

        let first = tuples[0]
            .deltas()
            .map(|d| (d.position, d.value))
            .filter(|(_, delta)| *delta != 0)
            .collect::<Vec<_>>();
        let second = tuples[1]
            .deltas()
            .map(|d| (d.position, d.value))
            .filter(|(_, delta)| *delta != 0)
            .collect::<Vec<_>>();

        assert_eq!(first, vec![(0, 10), (1, 20), (3, -5)]);
        assert_eq!(second, vec![(1, -6), (2, 8)]);
    }

    #[test]
    fn shared_points_when_beneficial() {
        let variations = vec![
            CvtDeltas::new(peaks(vec![F2Dot14::from_f32(1.0)]), vec![0, 3, 0, 4, 0]),
            CvtDeltas::new(peaks(vec![F2Dot14::from_f32(0.5)]), vec![0, 7, 0, 2, 0]),
            CvtDeltas::new(peaks(vec![F2Dot14::from_f32(-1.0)]), vec![1, 0, 2, 0, 3]),
        ];

        let table = Cvar::new(variations, 1).unwrap();
        let bytes = crate::dump_table(&table).unwrap();
        let read = read_fonts::tables::cvar::Cvar::read(FontData::new(&bytes)).unwrap();
        assert!(read.tuple_variation_count().shared_point_numbers());
    }
}
