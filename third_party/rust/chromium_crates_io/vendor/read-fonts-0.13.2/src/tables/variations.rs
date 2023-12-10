//! OpenType font variations common tables.

include!("../../generated/generated_variations.rs");

/// Outer and inner indices for reading from an [ItemVariationStore].
#[derive(Copy, Clone, Debug)]
pub struct DeltaSetIndex {
    /// Outer delta set index.
    pub outer: u16,
    /// Inner delta set index.
    pub inner: u16,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct TupleIndex(u16);

impl TupleIndex {
    /// Flag indicating that this tuple variation header includes an embedded
    /// peak tuple record, immediately after the tupleIndex field.
    ///
    /// If set, the low 12 bits of the tupleIndex value are ignored.
    ///
    /// Note that this must always be set within the 'cvar' table.
    pub const EMBEDDED_PEAK_TUPLE: u16 = 0x8000;

    /// Flag indicating that this tuple variation table applies to an
    /// intermediate region within the variation space.
    ///
    /// If set, the header includes the two intermediate-region, start and end
    /// tuple records, immediately after the peak tuple record (if present).
    pub const INTERMEDIATE_REGION: u16 = 0x4000;
    /// Flag indicating that the serialized data for this tuple variation table
    /// includes packed “point” number data.
    ///
    /// If set, this tuple variation table uses that number data; if clear,
    /// this tuple variation table uses shared number data found at the start
    /// of the serialized data for this glyph variation data or 'cvar' table.
    pub const PRIVATE_POINT_NUMBERS: u16 = 0x2000;
    //0x1000	Reserved	Reserved for future use — set to 0.
    //
    /// Mask for the low 12 bits to give the shared tuple records index.
    pub const TUPLE_INDEX_MASK: u16 = 0x0FFF;

    fn tuple_len(self, axis_count: u16, flag: usize) -> usize {
        match flag {
            0 => self.embedded_peak_tuple(),
            1 => self.intermediate_region(),
            _ => panic!("only 0 or 1 allowed here"),
        }
        .then_some(axis_count as usize)
        .unwrap_or_default()
    }

    pub fn bits(self) -> u16 {
        self.0
    }

    pub fn from_bits(bits: u16) -> Self {
        TupleIndex(bits)
    }

    /// `true` if the header includes an embedded peak tuple.
    pub fn embedded_peak_tuple(self) -> bool {
        (self.0 & Self::EMBEDDED_PEAK_TUPLE) != 0
    }

    /// `true` if the header includes the two intermediate region tuple records.
    pub fn intermediate_region(self) -> bool {
        (self.0 & Self::INTERMEDIATE_REGION) != 0
    }

    /// `true` if the data for this table includes packed point number data.
    pub fn private_point_numbers(self) -> bool {
        (self.0 & Self::PRIVATE_POINT_NUMBERS) != 0
    }

    pub fn tuple_records_index(self) -> Option<u16> {
        (!self.embedded_peak_tuple()).then_some(self.0 & Self::TUPLE_INDEX_MASK)
    }
}

impl types::Scalar for TupleIndex {
    type Raw = <u16 as types::Scalar>::Raw;
    fn to_raw(self) -> Self::Raw {
        self.0.to_raw()
    }
    fn from_raw(raw: Self::Raw) -> Self {
        let t = <u16>::from_raw(raw);
        Self(t)
    }
}

/// The 'tupleVariationCount' field of the [Tuple Variation Store Header][header]
///
/// The high 4 bits are flags, and the low 12 bits are the number of tuple
/// variation tables for this glyph. The count can be any number between 1 and 4095.
///
/// [header]: https://learn.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats#tuple-variation-store-header
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct TupleVariationCount(u16);

impl TupleVariationCount {
    /// Flag indicating that some or all tuple variation tables reference a
    /// shared set of “point” numbers.
    ///
    /// These shared numbers are represented as packed point number data at the
    /// start of the serialized data.
    pub const SHARED_POINT_NUMBERS: u16 = 0x8000;

    /// Mask for the low 12 bits to give the shared tuple records index.
    pub const COUNT_MASK: u16 = 0x0FFF;

    pub fn bits(self) -> u16 {
        self.0
    }

    pub fn from_bits(bits: u16) -> Self {
        Self(bits)
    }

    /// `true` if any tables reference a shared set of point numbers
    pub fn shared_point_numbers(self) -> bool {
        (self.0 & Self::SHARED_POINT_NUMBERS) != 0
    }

    pub fn count(self) -> u16 {
        self.0 & Self::COUNT_MASK
    }
}

impl types::Scalar for TupleVariationCount {
    type Raw = <u16 as types::Scalar>::Raw;
    fn to_raw(self) -> Self::Raw {
        self.0.to_raw()
    }
    fn from_raw(raw: Self::Raw) -> Self {
        let t = <u16>::from_raw(raw);
        Self(t)
    }
}

impl<'a> TupleVariationHeader<'a> {
    #[cfg(feature = "traversal")]
    fn traverse_tuple_index(&self) -> traversal::FieldType<'a> {
        self.tuple_index().0.into()
    }

    /// Peak tuple record for this tuple variation table — optional,
    /// determined by flags in the tupleIndex value.  Note that this
    /// must always be included in the 'cvar' table.
    pub fn peak_tuple(&self) -> Option<Tuple<'a>> {
        self.tuple_index().embedded_peak_tuple().then(|| {
            let range = self.shape.peak_tuple_byte_range();
            Tuple {
                values: self.data.read_array(range).unwrap(),
            }
        })
    }

    /// Intermediate start tuple record for this tuple variation table
    /// — optional, determined by flags in the tupleIndex value.
    pub fn intermediate_start_tuple(&self) -> Option<Tuple<'a>> {
        self.tuple_index().intermediate_region().then(|| {
            let range = self.shape.intermediate_start_tuple_byte_range();
            Tuple {
                values: self.data.read_array(range).unwrap(),
            }
        })
    }

    /// Intermediate end tuple record for this tuple variation table
    /// — optional, determined by flags in the tupleIndex value.
    pub fn intermediate_end_tuple(&self) -> Option<Tuple<'a>> {
        self.tuple_index().intermediate_region().then(|| {
            let range = self.shape.intermediate_end_tuple_byte_range();
            Tuple {
                values: self.data.read_array(range).unwrap(),
            }
        })
    }

    /// Compute the actual length of this table in bytes
    fn byte_len(&self, axis_count: u16) -> usize {
        const FIXED_LEN: usize = u16::RAW_BYTE_LEN + TupleIndex::RAW_BYTE_LEN;
        let tuple_byte_len = F2Dot14::RAW_BYTE_LEN * axis_count as usize;
        let index = self.tuple_index();
        FIXED_LEN
            + index
                .embedded_peak_tuple()
                .then_some(tuple_byte_len)
                .unwrap_or_default()
            + index
                .intermediate_region()
                .then_some(tuple_byte_len * 2)
                .unwrap_or_default()
    }
}

impl<'a> Tuple<'a> {
    pub fn len(&self) -> usize {
        self.values().len()
    }

    pub fn is_empty(&self) -> bool {
        self.values.is_empty()
    }

    pub fn get(&self, idx: usize) -> Option<F2Dot14> {
        self.values.get(idx).map(BigEndian::get)
    }
}

//FIXME: add an #[extra_traits(..)] attribute!
#[allow(clippy::derivable_impls)]
impl Default for Tuple<'_> {
    fn default() -> Self {
        Self {
            values: Default::default(),
        }
    }
}

/// [Packed "Point" Numbers](https://learn.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats#packed-point-numbers)
#[derive(Clone, Debug)]
pub struct PackedPointNumbers<'a> {
    data: FontData<'a>,
}

impl<'a> PackedPointNumbers<'a> {
    /// read point numbers off the front of this data, returning the remaining data
    pub fn split_off_front(data: FontData<'a>) -> (Self, FontData<'a>) {
        let this = PackedPointNumbers { data };
        let total_len = this.total_len();
        let remainder = data.split_off(total_len).unwrap_or_default();
        (this, remainder)
    }

    /// The number of points in this set
    pub fn count(&self) -> u16 {
        self.count_and_count_bytes().0
    }

    /// compute the count, and the number of bytes used to store it
    fn count_and_count_bytes(&self) -> (u16, usize) {
        match self.data.read_at::<u8>(0).unwrap_or(0) {
            0 => (0, 1),
            count @ 1..=127 => (count as u16, 1),
            _ => {
                // "If the high bit of the first byte is set, then a second byte is used.
                // The count is read from interpreting the two bytes as a big-endian
                // uint16 value with the high-order bit masked out."

                let count = self.data.read_at::<u16>(0).unwrap_or_default() & 0x7FFF;
                // a weird case where I'm following fonttools: if the 'use words' bit
                // is set, but the total count is still 0, treat it like 0 first byte
                if count == 0 {
                    (0, 2)
                } else {
                    (count & 0x7FFF, 2)
                }
            }
        }
    }

    /// the number of bytes to encode the packed point numbers
    fn total_len(&self) -> usize {
        let (n_points, mut n_bytes) = self.count_and_count_bytes();
        if n_points == 0 {
            return n_bytes;
        }
        let mut cursor = self.data.cursor();
        cursor.advance_by(n_bytes);

        let mut n_seen = 0;
        while n_seen < n_points {
            let Some((count, two_bytes)) = read_control_byte(&mut cursor) else {
                return n_bytes;
            };
            let word_size = 1 + usize::from(two_bytes);
            let run_size = word_size * count as usize;
            n_bytes += run_size + 1; // plus the control byte;
            cursor.advance_by(run_size);
            n_seen += count as u16;
        }

        n_bytes
    }

    /// Iterate over the packed points
    pub fn iter(&self) -> PackedPointNumbersIter<'a> {
        let (count, n_bytes) = self.count_and_count_bytes();
        let mut cursor = self.data.cursor();
        cursor.advance_by(n_bytes);
        PackedPointNumbersIter::new(count, cursor)
    }
}

/// An iterator over the packed point numbers data.
#[derive(Clone, Debug)]
pub struct PackedPointNumbersIter<'a> {
    count: u16,
    seen: u16,
    last_val: u16,
    current_run: PointRunIter<'a>,
}

impl<'a> PackedPointNumbersIter<'a> {
    fn new(count: u16, cursor: Cursor<'a>) -> Self {
        PackedPointNumbersIter {
            count,
            seen: 0,
            last_val: 0,
            current_run: PointRunIter {
                remaining: 0,
                two_bytes: false,
                cursor,
            },
        }
    }
}

/// Implements the logic for iterating over the individual runs
#[derive(Clone, Debug)]
struct PointRunIter<'a> {
    remaining: u8,
    two_bytes: bool,
    cursor: Cursor<'a>,
}

impl Iterator for PointRunIter<'_> {
    type Item = u16;

    fn next(&mut self) -> Option<Self::Item> {
        // if no items remain in this run, start the next one.
        while self.remaining == 0 {
            (self.remaining, self.two_bytes) = read_control_byte(&mut self.cursor)?;
        }

        self.remaining -= 1;
        if self.two_bytes {
            self.cursor.read().ok()
        } else {
            self.cursor.read::<u8>().ok().map(|v| v as u16)
        }
    }
}

/// returns the count and the 'uses_two_bytes' flag from the control byte
fn read_control_byte(cursor: &mut Cursor) -> Option<(u8, bool)> {
    let control: u8 = cursor.read().ok()?;
    let two_bytes = (control & 0x80) != 0;
    let count = (control & 0x7F) + 1;
    Some((count, two_bytes))
}

impl Iterator for PackedPointNumbersIter<'_> {
    type Item = u16;

    fn next(&mut self) -> Option<Self::Item> {
        // if our count is zero, we keep incrementing forever
        if self.count == 0 {
            let result = self.last_val;
            self.last_val = self.last_val.checked_add(1)?;
            return Some(result);
        }

        if self.count == self.seen {
            return None;
        }
        self.seen += 1;
        self.last_val += self.current_run.next()?;
        Some(self.last_val)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.count as usize, Some(self.count as usize))
    }
}

// completely unnecessary?
impl<'a> ExactSizeIterator for PackedPointNumbersIter<'a> {}

/// [Packed Deltas](https://learn.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats#packed-deltas)
#[derive(Clone, Debug)]
pub struct PackedDeltas<'a> {
    data: FontData<'a>,
    count: usize,
}

impl<'a> PackedDeltas<'a> {
    /// NOTE: this is unbounded, and assumes all of data is deltas.
    #[doc(hidden)] // used by tests in write-fonts
    pub fn new(data: FontData<'a>) -> Self {
        let count = DeltaRunIter::new(data.cursor()).count();
        Self { data, count }
    }

    pub(crate) fn count(&self) -> usize {
        self.count
    }

    #[doc(hidden)] // used by tests in write-fonts
    pub fn iter(&self) -> DeltaRunIter<'a> {
        DeltaRunIter::new(self.data.cursor())
    }
}

/// Implements the logic for iterating over the individual runs
#[derive(Clone, Debug)]
pub struct DeltaRunIter<'a> {
    remaining: u8,
    two_bytes: bool,
    are_zero: bool,
    cursor: Cursor<'a>,
}

impl<'a> DeltaRunIter<'a> {
    fn new(cursor: Cursor<'a>) -> Self {
        DeltaRunIter {
            remaining: 0,
            two_bytes: false,
            are_zero: false,
            cursor,
        }
    }
}

impl Iterator for DeltaRunIter<'_> {
    type Item = i16;

    fn next(&mut self) -> Option<Self::Item> {
        /// Flag indicating that this run contains no data,
        /// and that the deltas for this run are all zero.
        const DELTAS_ARE_ZERO: u8 = 0x80;
        /// Flag indicating the data type for delta values in the run.
        const DELTAS_ARE_WORDS: u8 = 0x40;
        /// Mask for the low 6 bits to provide the number of delta values in the run, minus one.
        const DELTA_RUN_COUNT_MASK: u8 = 0x3F;

        // if no items remain in this run, start the next one.
        // NOTE: we use `while` so we can sanely handle the case where some
        // run in the middle of the data has an explicit zero length
        //TODO: create a font with data of this shape and go crash some font parsers
        while self.remaining == 0 {
            let control: u8 = self.cursor.read().ok()?;
            self.are_zero = (control & DELTAS_ARE_ZERO) != 0;
            self.two_bytes = (control & DELTAS_ARE_WORDS) != 0;
            self.remaining = (control & DELTA_RUN_COUNT_MASK) + 1;
        }

        self.remaining -= 1;
        if self.are_zero {
            Some(0)
        } else if self.two_bytes {
            self.cursor.read().ok()
        } else {
            self.cursor.read::<i8>().ok().map(|v| v as i16)
        }
    }
}

/// A helper type for iterating over [`TupleVariationHeader`]s.
pub struct TupleVariationHeaderIter<'a> {
    data: FontData<'a>,
    n_headers: usize,
    current: usize,
    axis_count: u16,
}

impl<'a> TupleVariationHeaderIter<'a> {
    pub(crate) fn new(data: FontData<'a>, n_headers: usize, axis_count: u16) -> Self {
        Self {
            data,
            n_headers,
            current: 0,
            axis_count,
        }
    }
}

impl<'a> Iterator for TupleVariationHeaderIter<'a> {
    type Item = Result<TupleVariationHeader<'a>, ReadError>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current == self.n_headers {
            return None;
        }
        self.current += 1;
        let next = TupleVariationHeader::read(self.data, self.axis_count);
        let next_len = next
            .as_ref()
            .map(|table| table.byte_len(self.axis_count))
            .unwrap_or(0);
        self.data = self.data.split_off(next_len)?;
        Some(next)
    }
}

impl EntryFormat {
    pub fn entry_size(self) -> u8 {
        ((self.bits() & Self::MAP_ENTRY_SIZE_MASK.bits()) >> 4) + 1
    }

    pub fn bit_count(self) -> u8 {
        (self.bits() & Self::INNER_INDEX_BIT_COUNT_MASK.bits()) + 1
    }

    // called from codegen
    pub(crate) fn map_size(self, map_count: impl Into<u32>) -> usize {
        self.entry_size() as usize * map_count.into() as usize
    }
}

impl<'a> DeltaSetIndexMap<'a> {
    /// Returns the delta set index for the specified value.
    pub fn get(&self, index: u32) -> Result<DeltaSetIndex, ReadError> {
        let (entry_format, map_count, data) = match self {
            Self::Format0(fmt) => (fmt.entry_format(), fmt.map_count() as u32, fmt.map_data()),
            Self::Format1(fmt) => (fmt.entry_format(), fmt.map_count(), fmt.map_data()),
        };
        let entry_size = entry_format.entry_size();
        let data = FontData::new(data);
        // "if an index into the mapping array is used that is greater than or equal to
        // mapCount, then the last logical entry of the mapping array is used."
        // https://learn.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats
        // #associating-target-items-to-variation-data
        let index = index.min(map_count.saturating_sub(1));
        let offset = index as usize * entry_size as usize;
        let entry = match entry_size {
            1 => data.read_at::<u8>(offset)? as u32,
            2 => data.read_at::<u16>(offset)? as u32,
            3 => data.read_at::<Uint24>(offset)?.into(),
            4 => data.read_at::<u32>(offset)?,
            _ => {
                return Err(ReadError::MalformedData(
                    "invalid entry size in DeltaSetIndexMap",
                ))
            }
        };
        let bit_count = entry_format.bit_count();
        Ok(DeltaSetIndex {
            outer: (entry >> bit_count) as u16,
            inner: (entry & ((1 << bit_count) - 1)) as u16,
        })
    }
}

impl<'a> ItemVariationStore<'a> {
    /// Computes the delta value for the specified index and set of normalized
    /// variation coordinates.
    pub fn compute_delta(
        &self,
        index: DeltaSetIndex,
        coords: &[F2Dot14],
    ) -> Result<i32, ReadError> {
        let data = match self.item_variation_data().get(index.outer as usize) {
            Some(data) => data?,
            None => return Ok(0),
        };
        let regions = self.variation_region_list()?.variation_regions();
        let region_indices = data.region_indexes();
        // Compute deltas with 64-bit precision.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/7ab541a2/src/truetype/ttgxvar.c#L1094>
        let mut accum = 0i64;
        for (i, region_delta) in data.delta_set(index.inner).enumerate() {
            let region_index = region_indices
                .get(i)
                .ok_or(ReadError::MalformedData(
                    "invalid delta sets in ItemVariationStore",
                ))?
                .get() as usize;
            let region = regions.get(region_index)?;
            let scalar = region.compute_scalar(coords);
            accum += region_delta as i64 * scalar.to_bits() as i64;
        }
        Ok(((accum + 0x8000) >> 16) as i32)
    }
}

impl<'a> VariationRegion<'a> {
    /// Computes a scalar value for this region and the specified
    /// normalized variation coordinates.
    pub fn compute_scalar(&self, coords: &[F2Dot14]) -> Fixed {
        const ZERO: Fixed = Fixed::ZERO;
        let mut scalar = Fixed::ONE;
        for (i, axis_coords) in self.region_axes().iter().enumerate() {
            let coord = coords.get(i).map(|coord| coord.to_fixed()).unwrap_or(ZERO);
            let start = axis_coords.start_coord.get().to_fixed();
            let end = axis_coords.end_coord.get().to_fixed();
            let peak = axis_coords.peak_coord.get().to_fixed();
            if start > peak || peak > end || peak == ZERO || start < ZERO && end > ZERO {
                continue;
            } else if coord < start || coord > end {
                return ZERO;
            } else if coord == peak {
                continue;
            } else if coord < peak {
                scalar = scalar.mul_div(coord - start, peak - start);
            } else {
                scalar = scalar.mul_div(end - coord, end - peak);
            }
        }
        scalar
    }
}

impl<'a> ItemVariationData<'a> {
    /// Returns an iterator over the per-region delta values for the specified
    /// inner index.
    pub fn delta_set(&self, inner_index: u16) -> impl Iterator<Item = i32> + 'a + Clone {
        let word_delta_count = self.word_delta_count();
        let long_words = word_delta_count & 0x8000 != 0;
        let (word_size, small_size) = if long_words { (4, 2) } else { (2, 1) };
        let word_delta_count = word_delta_count & 0x7FFF;
        let region_count = self.region_index_count() as usize;
        let row_size = word_delta_count as usize * word_size
            + region_count.saturating_sub(word_delta_count as usize) * small_size;
        let offset = row_size * inner_index as usize;
        ItemDeltas {
            cursor: FontData::new(self.delta_sets())
                .slice(offset..)
                .unwrap_or_default()
                .cursor(),
            word_delta_count,
            long_words,
            len: region_count as u16,
            pos: 0,
        }
    }
}

#[derive(Clone)]
struct ItemDeltas<'a> {
    cursor: Cursor<'a>,
    word_delta_count: u16,
    long_words: bool,
    len: u16,
    pos: u16,
}

impl<'a> Iterator for ItemDeltas<'a> {
    type Item = i32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos >= self.len {
            return None;
        }
        let pos = self.pos;
        self.pos += 1;
        let value = match (pos >= self.word_delta_count, self.long_words) {
            (true, true) | (false, false) => self.cursor.read::<i16>().ok()? as i32,
            (true, false) => self.cursor.read::<i8>().ok()? as i32,
            (false, true) => self.cursor.read::<i32>().ok()?,
        };
        Some(value)
    }
}

pub(crate) fn advance_delta(
    dsim: Option<Result<DeltaSetIndexMap, ReadError>>,
    ivs: Result<ItemVariationStore, ReadError>,
    glyph_id: GlyphId,
    coords: &[F2Dot14],
) -> Result<Fixed, ReadError> {
    let gid = glyph_id.to_u16();
    let ix = match dsim {
        Some(Ok(dsim)) => dsim.get(gid as u32)?,
        _ => DeltaSetIndex {
            outer: 0,
            inner: gid,
        },
    };
    Ok(Fixed::from_i32(ivs?.compute_delta(ix, coords)?))
}

pub(crate) fn item_delta(
    dsim: Option<Result<DeltaSetIndexMap, ReadError>>,
    ivs: Result<ItemVariationStore, ReadError>,
    glyph_id: GlyphId,
    coords: &[F2Dot14],
) -> Result<Fixed, ReadError> {
    let gid = glyph_id.to_u16();
    let ix = match dsim {
        Some(Ok(dsim)) => dsim.get(gid as u32)?,
        _ => return Err(ReadError::NullOffset),
    };
    Ok(Fixed::from_i32(ivs?.compute_delta(ix, coords)?))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};

    #[test]
    fn ivs_regions() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let hvar = font.hvar().expect("missing HVAR table");
        let ivs = hvar
            .item_variation_store()
            .expect("missing item variation store in HVAR");
        let region_list = ivs.variation_region_list().expect("missing region list!");
        let regions = region_list.variation_regions();
        let expected = &[
            // start_coord, peak_coord, end_coord
            vec![[-1.0f32, -1.0, 0.0]],
            vec![[0.0, 1.0, 1.0]],
        ][..];
        let region_coords = regions
            .iter()
            .map(|region| {
                region
                    .unwrap()
                    .region_axes()
                    .iter()
                    .map(|coords| {
                        [
                            coords.start_coord().to_f32(),
                            coords.peak_coord().to_f32(),
                            coords.end_coord().to_f32(),
                        ]
                    })
                    .collect::<Vec<_>>()
            })
            .collect::<Vec<_>>();
        assert_eq!(expected, &region_coords);
    }

    // adapted from https://github.com/fonttools/fonttools/blob/f73220816264fc383b8a75f2146e8d69e455d398/Tests/ttLib/tables/TupleVariation_test.py#L492
    #[test]
    fn packed_points() {
        fn decode_points(bytes: &[u8]) -> Option<Vec<u16>> {
            let data = FontData::new(bytes);
            let packed = PackedPointNumbers { data };
            if packed.count() == 0 {
                None
            } else {
                Some(packed.iter().collect())
            }
        }

        assert_eq!(decode_points(&[0]), None);
        // all points in glyph (in overly verbose encoding, not explicitly prohibited by spec)
        assert_eq!(decode_points(&[0x80, 0]), None);
        // 2 points; first run: [9, 9+6]
        assert_eq!(decode_points(&[0x02, 0x01, 0x09, 0x06]), Some(vec![9, 15]));
        // 2 points; first run: [0xBEEF, 0xCAFE]. (0x0C0F = 0xCAFE - 0xBEEF)
        assert_eq!(
            decode_points(&[0x02, 0x81, 0xbe, 0xef, 0x0c, 0x0f]),
            Some(vec![0xbeef, 0xcafe])
        );
        // 1 point; first run: [7]
        assert_eq!(decode_points(&[0x01, 0, 0x07]), Some(vec![7]));
        // 1 point; first run: [7] in overly verbose encoding
        assert_eq!(decode_points(&[0x01, 0x80, 0, 0x07]), Some(vec![7]));
        // 1 point; first run: [65535]; requires words to be treated as unsigned numbers
        assert_eq!(decode_points(&[0x01, 0x80, 0xff, 0xff]), Some(vec![65535]));
        // 4 points; first run: [7, 8]; second run: [255, 257]. 257 is stored in delta-encoded bytes (0xFF + 2).
        assert_eq!(
            decode_points(&[0x04, 1, 7, 1, 1, 0xff, 2]),
            Some(vec![7, 8, 263, 265])
        );
    }

    #[test]
    fn packed_point_byte_len() {
        fn count_bytes(bytes: &[u8]) -> usize {
            let packed = PackedPointNumbers {
                data: FontData::new(bytes),
            };
            packed.total_len()
        }

        static CASES: &[&[u8]] = &[
            &[0],
            &[0x80, 0],
            &[0x02, 0x01, 0x09, 0x06],
            &[0x02, 0x81, 0xbe, 0xef, 0x0c, 0x0f],
            &[0x01, 0, 0x07],
            &[0x01, 0x80, 0, 0x07],
            &[0x01, 0x80, 0xff, 0xff],
            &[0x04, 1, 7, 1, 1, 0xff, 2],
        ];

        for case in CASES {
            assert_eq!(count_bytes(case), case.len(), "{case:?}");
        }
    }

    // https://github.com/fonttools/fonttools/blob/c30a6355ffdf7f09d31e7719975b4b59bac410af/Tests/ttLib/tables/TupleVariation_test.py#L670
    #[test]
    fn packed_deltas() {
        static INPUT: FontData = FontData::new(&[0x83, 0x40, 0x01, 0x02, 0x01, 0x81, 0x80]);

        let deltas = PackedDeltas::new(INPUT);
        assert_eq!(deltas.count, 7);
        assert_eq!(
            deltas.iter().collect::<Vec<_>>(),
            &[0, 0, 0, 0, 258, -127, -128]
        );

        assert_eq!(
            PackedDeltas::new(FontData::new(&[0x81]))
                .iter()
                .collect::<Vec<_>>(),
            &[0, 0,]
        );
    }

    // https://learn.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats#packed-deltas
    #[test]
    fn packed_deltas_spec() {
        static INPUT: FontData = FontData::new(&[
            0x03, 0x0A, 0x97, 0x00, 0xC6, 0x87, 0x41, 0x10, 0x22, 0xFB, 0x34,
        ]);
        static EXPECTED: &[i16] = &[10, -105, 0, -58, 0, 0, 0, 0, 0, 0, 0, 0, 4130, -1228];

        let deltas = PackedDeltas::new(INPUT);
        assert_eq!(deltas.count, EXPECTED.len());
        assert_eq!(deltas.iter().collect::<Vec<_>>(), EXPECTED);
    }

    #[test]
    fn packed_point_split() {
        static INPUT: FontData =
            FontData::new(&[2, 1, 1, 2, 1, 205, 143, 1, 8, 0, 1, 202, 59, 1, 255, 0]);
        let (points, data) = PackedPointNumbers::split_off_front(INPUT);
        assert_eq!(points.count(), 2);
        assert_eq!(points.iter().collect::<Vec<_>>(), &[1, 3]);
        assert_eq!(points.total_len(), 4);
        assert_eq!(data.len(), INPUT.len() - 4);
    }

    #[test]
    fn packed_points_dont_panic() {
        // a single '0' byte means that there are deltas for all points
        static ALL_POINTS: FontData = FontData::new(&[0]);
        let (all_points, _) = PackedPointNumbers::split_off_front(ALL_POINTS);
        // in which case the iterator just keeps incrementing until u16::MAX
        assert_eq!(all_points.iter().count(), u16::MAX as _);
    }
}
