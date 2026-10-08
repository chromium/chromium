//! A GPOS ValueRecord

use font_types::Nullable;
use types::{FixedSize, Offset16};

use super::ValueFormat;
use crate::{tables::layout::DeviceOrVariationIndex, ResolveNullableOffset};

use crate::{ComputeSize, FontData, FontReadAt, ReadArgs, ReadError};

impl ValueFormat {
    /// A mask with all the device/variation index bits set
    pub const ANY_DEVICE_OR_VARIDX: Self = ValueFormat {
        bits: 0x0010 | 0x0020 | 0x0040 | 0x0080,
    };

    /// Return the number of bytes required to store a [`ValueRecord`] in this format.
    #[inline]
    pub fn record_byte_len(self) -> usize {
        self.bits().count_ones() as usize * u16::RAW_BYTE_LEN
    }
}

/// A GPOS ValueRecord, with fields read on demand.
///
/// The contents of a value record are described by a [`ValueFormat`] stored in
/// the parent table, so a record cannot be located or interpreted on its own.
/// This type stores the position of the record within its parent table's data
/// alongside that format, and reads individual fields only when they are asked
/// for; constructing one performs no reads at all.
#[derive(Copy, Clone, Default)]
pub struct ValueRecord<'a> {
    /// The offset data of the table *containing* the record.
    data: FontData<'a>,
    /// The position of the record within `data`.
    offset: u32,
    format: ValueFormat,
}

impl<'a> ValueRecord<'a> {
    /// Creates a value record positioned at `offset` within `data`.
    ///
    /// `data` must be the offset data of the table containing the record, and
    /// not merely the bytes of the record itself: the device and variation
    /// index offsets in a value record are resolved relative to that table.
    #[inline]
    pub fn new(data: FontData<'a>, offset: usize, format: ValueFormat) -> Self {
        Self {
            data,
            // an offset that doesn't fit is out of bounds by definition, and
            // saturating here lets every subsequent read fail cleanly
            offset: u32::try_from(offset).unwrap_or(u32::MAX),
            format,
        }
    }

    /// The format describing which fields this record contains.
    #[inline]
    pub fn format(&self) -> ValueFormat {
        self.format
    }

    /// The number of bytes occupied by this record.
    #[inline]
    pub fn byte_len(&self) -> usize {
        self.format.record_byte_len()
    }

    /// Returns `true` if this record contains no fields.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.format.is_empty()
    }

    /// The offset data of the table containing this record.
    #[inline]
    pub fn offset_data(&self) -> FontData<'a> {
        self.data
    }

    /// The position of this record within [`offset_data`](Self::offset_data).
    #[inline]
    pub fn offset(&self) -> usize {
        self.offset as usize
    }

    #[inline]
    pub fn x_placement(&self) -> Option<i16> {
        self.read_i16(ValueFormat::X_PLACEMENT)
    }

    #[inline]
    pub fn y_placement(&self) -> Option<i16> {
        self.read_i16(ValueFormat::Y_PLACEMENT)
    }

    #[inline]
    pub fn x_advance(&self) -> Option<i16> {
        self.read_i16(ValueFormat::X_ADVANCE)
    }

    #[inline]
    pub fn y_advance(&self) -> Option<i16> {
        self.read_i16(ValueFormat::Y_ADVANCE)
    }

    #[inline]
    pub fn x_placement_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.read_device(ValueFormat::X_PLACEMENT_DEVICE)
    }

    #[inline]
    pub fn y_placement_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.read_device(ValueFormat::Y_PLACEMENT_DEVICE)
    }

    #[inline]
    pub fn x_advance_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.read_device(ValueFormat::X_ADVANCE_DEVICE)
    }

    #[inline]
    pub fn y_advance_device(&self) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.read_device(ValueFormat::Y_ADVANCE_DEVICE)
    }

    /// Returns the raw, unresolved offset for the given device field.
    ///
    /// The returned offset is relative to [`offset_data`](Self::offset_data).
    /// Returns `None` if the field is not present in this record's format, or
    /// if the record is truncated.
    #[inline]
    pub fn device_offset(&self, field: ValueFormat) -> Option<Nullable<Offset16>> {
        self.data.read_at(self.field_offset(field)?).ok()
    }

    /// The raw bytes of this record, or `None` if the data is truncated.
    #[inline]
    pub fn bytes(&self) -> Option<&'a [u8]> {
        let start = self.offset as usize;
        self.data
            .as_bytes()
            .get(start..start.checked_add(self.byte_len())?)
    }

    /// Returns the position of `field` within [`offset_data`](Self::offset_data),
    /// or `None` if this record's format doesn't include it.
    ///
    /// Fields are laid out in the order of their format bits and each occupies
    /// two bytes, so a field's position is fixed by the number of lower format
    /// bits that are set.
    #[inline]
    fn field_offset(&self, field: ValueFormat) -> Option<usize> {
        if !self.format.contains(field) {
            return None;
        }
        let preceding = (self.format.bits() & (field.bits() - 1)).count_ones() as usize;
        Some(self.offset as usize + preceding * u16::RAW_BYTE_LEN)
    }

    #[inline]
    fn read_i16(&self, field: ValueFormat) -> Option<i16> {
        self.data.read_at(self.field_offset(field)?).ok()
    }

    #[inline]
    fn read_device(
        &self,
        field: ValueFormat,
    ) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        let pos = self.field_offset(field)?;
        match self.data.read_at::<Nullable<Offset16>>(pos) {
            Ok(offset) => offset.resolve(self.data),
            Err(err) => Some(Err(err)),
        }
    }
}

impl ReadArgs for ValueRecord<'_> {
    type Args = ValueFormat;
}

impl ComputeSize for ValueRecord<'_> {
    #[inline]
    fn compute_size(args: ValueFormat) -> Result<usize, ReadError> {
        Ok(args.record_byte_len())
    }
}

impl<'a> FontReadAt<'a> for ValueRecord<'a> {
    #[inline]
    fn read_at(data: FontData<'a>, offset: usize, args: ValueFormat) -> Result<Self, ReadError> {
        Ok(Self::new(data, offset, args))
    }
}

/// Two records are equal when they describe the same positioning: same format,
/// and the same bytes for the fields that format selects.
impl PartialEq for ValueRecord<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.format == other.format && self.bytes() == other.bytes()
    }
}

impl Eq for ValueRecord<'_> {}

impl std::fmt::Debug for ValueRecord<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let mut f = f.debug_struct("ValueRecord");
        self.x_placement().map(|x| f.field("x_placement", &x));
        self.y_placement().map(|y| f.field("y_placement", &y));
        self.x_advance().map(|x| f.field("x_advance", &x));
        self.y_advance().map(|y| f.field("y_advance", &y));
        for (name, field) in [
            ("x_placement_device", ValueFormat::X_PLACEMENT_DEVICE),
            ("y_placement_device", ValueFormat::Y_PLACEMENT_DEVICE),
            ("x_advance_device", ValueFormat::X_ADVANCE_DEVICE),
            ("y_advance_device", ValueFormat::Y_ADVANCE_DEVICE),
        ] {
            match self.device_offset(field) {
                Some(offset) if !offset.is_null() => {
                    f.field(name, &offset);
                }
                _ => (),
            }
        }
        f.finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanity_check_format_const() {
        let format = ValueFormat::X_ADVANCE_DEVICE
            | ValueFormat::Y_ADVANCE_DEVICE
            | ValueFormat::Y_PLACEMENT_DEVICE
            | ValueFormat::X_PLACEMENT_DEVICE;
        assert_eq!(format, ValueFormat::ANY_DEVICE_OR_VARIDX);
        assert_eq!(format.record_byte_len(), 4 * 2);
    }

    /// Walks the fields in order, the way the spec describes the layout, and
    /// returns the position of each present field. This is the straightforward
    /// reading of the format that [`ValueRecord`] replaces with a popcount.
    fn reference_field_positions(format: ValueFormat) -> Vec<(ValueFormat, usize)> {
        let mut pos = 0;
        let mut out = Vec::new();
        for field in [
            ValueFormat::X_PLACEMENT,
            ValueFormat::Y_PLACEMENT,
            ValueFormat::X_ADVANCE,
            ValueFormat::Y_ADVANCE,
            ValueFormat::X_PLACEMENT_DEVICE,
            ValueFormat::Y_PLACEMENT_DEVICE,
            ValueFormat::X_ADVANCE_DEVICE,
            ValueFormat::Y_ADVANCE_DEVICE,
        ] {
            if format.contains(field) {
                out.push((field, pos));
                pos += 2;
            }
        }
        out
    }

    /// A value record locates its fields with a popcount over the format bits.
    /// That must agree with walking the fields in order, for every combination
    /// of bits.
    #[test]
    fn field_offsets_match_sequential_layout() {
        // give every field slot a distinct, recognizable value
        let bytes: Vec<u8> = (0..8u16).flat_map(|i| (0x1100 + i).to_be_bytes()).collect();
        // pad the front so we exercise a non-zero record offset
        const PAD: usize = 6;
        let mut padded = vec![0u8; PAD];
        padded.extend_from_slice(&bytes);
        let data = FontData::new(&padded);

        for bits in 0..=u8::MAX {
            let format = ValueFormat { bits: bits as u16 };
            let record = ValueRecord::new(data, PAD, format);

            assert_eq!(record.format(), format);
            assert_eq!(record.byte_len(), format.record_byte_len());

            let expected = reference_field_positions(format);
            assert_eq!(expected.len() * 2, record.byte_len(), "{format:?}");

            for (field, offset) in expected {
                // the value planted at that position in the record
                let want = 0x1100u16 + (offset / 2) as u16;
                let got = match field {
                    ValueFormat::X_PLACEMENT => record.x_placement().map(|v| v as u16),
                    ValueFormat::Y_PLACEMENT => record.y_placement().map(|v| v as u16),
                    ValueFormat::X_ADVANCE => record.x_advance().map(|v| v as u16),
                    ValueFormat::Y_ADVANCE => record.y_advance().map(|v| v as u16),
                    other => record
                        .device_offset(other)
                        .map(|off| off.offset().to_u32() as u16),
                };
                assert_eq!(got, Some(want), "{format:?} {field:?} at {offset}");
            }

            // absent fields must report absent, not read a neighbour
            for field in [
                ValueFormat::X_PLACEMENT,
                ValueFormat::Y_PLACEMENT,
                ValueFormat::X_ADVANCE,
                ValueFormat::Y_ADVANCE,
            ] {
                if !format.contains(field) {
                    let got = match field {
                        ValueFormat::X_PLACEMENT => record.x_placement(),
                        ValueFormat::Y_PLACEMENT => record.y_placement(),
                        ValueFormat::X_ADVANCE => record.x_advance(),
                        _ => record.y_advance(),
                    };
                    assert_eq!(got, None, "{format:?} {field:?}");
                }
            }
            for field in [
                ValueFormat::X_PLACEMENT_DEVICE,
                ValueFormat::Y_PLACEMENT_DEVICE,
                ValueFormat::X_ADVANCE_DEVICE,
                ValueFormat::Y_ADVANCE_DEVICE,
            ] {
                if !format.contains(field) {
                    assert_eq!(record.device_offset(field), None, "{format:?} {field:?}");
                }
            }
        }
    }

    /// Reads past the end of the data must fail cleanly rather than panic or
    /// read a neighbouring field.
    #[test]
    fn lazy_fields_out_of_bounds() {
        let format = ValueFormat::X_PLACEMENT | ValueFormat::Y_ADVANCE;
        // only enough room for the first of the two fields
        let bytes = [0u8, 1, 0, 2];
        let lazy = ValueRecord::new(FontData::new(&bytes), 2, format);
        assert_eq!(lazy.x_placement(), Some(2));
        assert_eq!(lazy.y_advance(), None);

        // an offset that can't even be represented
        let huge = ValueRecord::new(FontData::new(&bytes), usize::MAX, format);
        assert_eq!(huge.x_placement(), None);
    }
}
