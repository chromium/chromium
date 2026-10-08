//! impl subset() for ValueRecord

use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{ValueFormat, ValueRecord},
            layout::DeviceOrVariationIndex,
        },
        ReadError, ResolveOffset,
    },
    types::Offset16,
};

/// The device fields of a value record, in on-disk order.
const NON_DEVICE_FIELDS: [ValueFormat; 4] = [
    ValueFormat::X_PLACEMENT,
    ValueFormat::Y_PLACEMENT,
    ValueFormat::X_ADVANCE,
    ValueFormat::Y_ADVANCE,
];

/// The device fields of a value record, in on-disk order.
const DEVICE_FIELDS: [ValueFormat; 4] = [
    ValueFormat::X_PLACEMENT_DEVICE,
    ValueFormat::Y_PLACEMENT_DEVICE,
    ValueFormat::X_ADVANCE_DEVICE,
    ValueFormat::Y_ADVANCE_DEVICE,
];

// Returns the number of 1 bits in a u8
#[inline]
fn popcount8(v: u8) -> u8 {
    const POPCOUNT4: [u8; 16] = [0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4];
    POPCOUNT4[(v & 0xF) as usize] + POPCOUNT4[(v >> 4) as usize]
}

// use faster popcount8 that only processes the lowest 8 bits
// Harfbuzz ref: <https://github.com/harfbuzz/harfbuzz/blob/f279195ce7e0a04c16576214d524d2629ea0aa79/src/OT/Layout/GPOS/ValueFormat.hh#L66>
pub(super) fn compute_record_len(value_format: ValueFormat) -> usize {
    let v = value_format.bits() as u8;
    popcount8(v) as usize * 2
}

pub(crate) fn compute_effective_format(
    value_record: &ValueRecord,
    strip_hints: bool,
    strip_empty: bool,
) -> Result<ValueFormat, ReadError> {
    let mut effective = value_record.format();

    if strip_hints {
        effective -= ValueFormat::ANY_DEVICE_OR_VARIDX;
    }

    if !strip_empty {
        return Ok(effective);
    }

    let mut offset = value_record.offset();
    let value_format = value_record.format();
    let font_data = value_record.offset_data();
    for &field in NON_DEVICE_FIELDS.iter().chain(DEVICE_FIELDS.iter()) {
        if !value_format.contains(field) {
            continue;
        }

        let value = font_data.read_at::<u16>(offset)?;
        if value == 0 {
            effective -= field;
        }
        offset += 2;
    }

    Ok(effective)
}

impl<'a> SubsetTable<'a> for ValueRecord<'_> {
    type ArgsForSubset = ValueFormat;
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        new_format: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if new_format.is_empty() {
            return Ok(());
        }

        let mut offset = self.offset();
        let font_data = self.offset_data();
        let value_format = self.format();

        for field in NON_DEVICE_FIELDS {
            if !value_format.contains(field) {
                continue;
            }
            if !new_format.contains(field) {
                offset += 2;
                continue;
            }

            let value_bytes = font_data
                .slice(offset..offset + 2)
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            s.embed_bytes(value_bytes.as_bytes())?;
            offset += 2;
        }

        for field in DEVICE_FIELDS {
            if !value_format.contains(field) {
                continue;
            }
            if !new_format.contains(field) {
                offset += 2;
                continue;
            }

            let offset_pos = s.embed(0_u16)?;
            let device_offset = font_data
                .read_at::<Offset16>(offset)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            if !device_offset.is_null() {
                let device = device_offset
                    .resolve::<DeviceOrVariationIndex>(font_data)
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                Offset16::serialize_subset(
                    &device,
                    s,
                    plan,
                    &plan.layout_varidx_delta_map,
                    offset_pos,
                )
                .is_empty()?;
            }
            offset += 2;
        }
        Ok(())
    }
}

impl CollectVariationIndices for ValueRecord<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if !self.format().intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
            return;
        }

        let mut offset = self.offset();
        let value_format = self.format();
        for field in NON_DEVICE_FIELDS {
            if value_format.contains(field) {
                offset += 2;
            }
        }

        let font_data = self.offset_data();
        for field in DEVICE_FIELDS {
            if !value_format.contains(field) {
                continue;
            }

            let Ok(device_offset) = font_data.read_at::<Offset16>(offset) else {
                return;
            };

            if device_offset.is_null() {
                offset += 2;
                continue;
            }

            let Ok(device) = device_offset.resolve::<DeviceOrVariationIndex>(font_data) else {
                return;
            };
            device.collect_variation_indices(plan, varidx_set);
            offset += 2;
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::FontData;

    const ALL: ValueFormat = ValueFormat::from_bits_truncate(0x00FF);

    /// A record in `ALL` format: four values then four device offsets.
    fn record(bytes: &[u8]) -> ValueRecord<'_> {
        ValueRecord::new(FontData::new(bytes), 0, ALL)
    }

    /// Bytes for a record whose every field is zero.
    const EMPTY: [u8; 16] = [0; 16];

    /// x_placement = 1, x_placement_device -> 12; everything else zero.
    const PARTLY_SET: [u8; 16] = [0, 1, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0];

    #[test]
    fn keeps_everything_when_stripping_nothing() {
        // with neither flag set the format is reported as-is, even for fields
        // that are zero: the caller has not asked for anything to be dropped
        assert_eq!(
            compute_effective_format(&record(&EMPTY), false, false),
            Ok(ALL)
        );
        assert_eq!(
            compute_effective_format(&record(&PARTLY_SET), false, false),
            Ok(ALL)
        );
    }

    #[test]
    fn strip_empty_drops_zero_values_and_null_offsets() {
        // an all-zero record keeps nothing
        assert_eq!(
            compute_effective_format(&record(&EMPTY), false, true),
            Ok(ValueFormat::empty())
        );
        // and only the two fields that are actually set survive
        assert_eq!(
            compute_effective_format(&record(&PARTLY_SET), false, true),
            Ok(ValueFormat::X_PLACEMENT | ValueFormat::X_PLACEMENT_DEVICE)
        );
    }

    #[test]
    fn strip_hints_drops_devices_however_they_are_set() {
        // device fields go whether or not their offsets are null, and whether
        // or not empty fields are being stripped
        assert_eq!(
            compute_effective_format(&record(&PARTLY_SET), true, false),
            Ok(ALL - ValueFormat::ANY_DEVICE_OR_VARIDX)
        );
        assert_eq!(
            compute_effective_format(&record(&PARTLY_SET), true, true),
            Ok(ValueFormat::X_PLACEMENT)
        );
    }

    /// Fields absent from the format are never reported present, whatever the
    /// bytes underneath happen to say.
    #[test]
    fn absent_fields_stay_absent() {
        let format = ValueFormat::Y_ADVANCE | ValueFormat::Y_ADVANCE_DEVICE;
        let bytes = [0u8, 7, 0, 12];
        let rec = ValueRecord::new(FontData::new(&bytes), 0, format);
        assert_eq!(compute_effective_format(&rec, false, true), Ok(format));
        assert_eq!(
            compute_effective_format(&rec, true, true),
            Ok(ValueFormat::Y_ADVANCE)
        );
    }
}
