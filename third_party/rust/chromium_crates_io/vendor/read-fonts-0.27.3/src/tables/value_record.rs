//! A GPOS ValueRecord

use font_types::Nullable;
use types::{BigEndian, FixedSize, Offset16};

use super::ValueFormat;
use crate::{tables::layout::DeviceOrVariationIndex, ResolveNullableOffset};

#[cfg(feature = "experimental_traverse")]
use crate::traversal::{Field, FieldType, RecordResolver, SomeRecord};
use crate::{ComputeSize, FontData, FontReadWithArgs, ReadArgs, ReadError};

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

/// A Positioning ValueRecord.
///
/// NOTE: we create these manually, since parsing is weird and depends on the
/// associated valueformat. That said, this isn't a great representation?
/// we could definitely do something much more in the zero-copy mode..
#[derive(Clone, Default, Eq)]
pub struct ValueRecord {
    pub x_placement: Option<BigEndian<i16>>,
    pub y_placement: Option<BigEndian<i16>>,
    pub x_advance: Option<BigEndian<i16>>,
    pub y_advance: Option<BigEndian<i16>>,
    pub x_placement_device: BigEndian<Nullable<Offset16>>,
    pub y_placement_device: BigEndian<Nullable<Offset16>>,
    pub x_advance_device: BigEndian<Nullable<Offset16>>,
    pub y_advance_device: BigEndian<Nullable<Offset16>>,
    #[doc(hidden)]
    // exposed so that we can preserve format when we round-trip a value record
    pub format: ValueFormat,
}

// we ignore the format for the purpose of equality testing, it's redundant
impl PartialEq for ValueRecord {
    fn eq(&self, other: &Self) -> bool {
        self.x_placement == other.x_placement
            && self.y_placement == other.y_placement
            && self.x_advance == other.x_advance
            && self.y_advance == other.y_advance
            && self.x_placement_device == other.x_placement_device
            && self.y_placement_device == other.y_placement_device
            && self.x_advance_device == other.x_advance_device
            && self.y_advance_device == other.y_advance_device
    }
}

impl ValueRecord {
    pub fn read(data: FontData, format: ValueFormat) -> Result<Self, ReadError> {
        let mut this = ValueRecord {
            format,
            ..Default::default()
        };
        let mut cursor = data.cursor();

        if format.contains(ValueFormat::X_PLACEMENT) {
            this.x_placement = Some(cursor.read_be()?);
        }
        if format.contains(ValueFormat::Y_PLACEMENT) {
            this.y_placement = Some(cursor.read_be()?);
        }
        if format.contains(ValueFormat::X_ADVANCE) {
            this.x_advance = Some(cursor.read_be()?);
        }
        if format.contains(ValueFormat::Y_ADVANCE) {
            this.y_advance = Some(cursor.read_be()?);
        }
        if format.contains(ValueFormat::X_PLACEMENT_DEVICE) {
            this.x_placement_device = cursor.read_be()?;
        }
        if format.contains(ValueFormat::Y_PLACEMENT_DEVICE) {
            this.y_placement_device = cursor.read_be()?;
        }
        if format.contains(ValueFormat::X_ADVANCE_DEVICE) {
            this.x_advance_device = cursor.read_be()?;
        }
        if format.contains(ValueFormat::Y_ADVANCE_DEVICE) {
            this.y_advance_device = cursor.read_be()?;
        }
        Ok(this)
    }

    pub fn x_placement(&self) -> Option<i16> {
        self.x_placement.map(|val| val.get())
    }

    pub fn y_placement(&self) -> Option<i16> {
        self.y_placement.map(|val| val.get())
    }

    pub fn x_advance(&self) -> Option<i16> {
        self.x_advance.map(|val| val.get())
    }

    pub fn y_advance(&self) -> Option<i16> {
        self.y_advance.map(|val| val.get())
    }

    pub fn x_placement_device<'a>(
        &self,
        data: FontData<'a>,
    ) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.x_placement_device.get().resolve(data)
    }

    pub fn y_placement_device<'a>(
        &self,
        data: FontData<'a>,
    ) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.y_placement_device.get().resolve(data)
    }

    pub fn x_advance_device<'a>(
        &self,
        data: FontData<'a>,
    ) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.x_advance_device.get().resolve(data)
    }

    pub fn y_advance_device<'a>(
        &self,
        data: FontData<'a>,
    ) -> Option<Result<DeviceOrVariationIndex<'a>, ReadError>> {
        self.y_advance_device.get().resolve(data)
    }
}

impl ReadArgs for ValueRecord {
    type Args = ValueFormat;
}

impl<'a> FontReadWithArgs<'a> for ValueRecord {
    fn read_with_args(data: FontData<'a>, args: &Self::Args) -> Result<Self, ReadError> {
        ValueRecord::read(data, *args)
    }
}

impl std::fmt::Debug for ValueRecord {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let mut f = f.debug_struct("ValueRecord");
        self.x_placement.map(|x| f.field("x_placement", &x));
        self.y_placement.map(|y| f.field("y_placement", &y));
        self.x_advance.map(|x| f.field("x_advance", &x));
        self.y_advance.map(|y| f.field("y_advance", &y));
        if !self.x_placement_device.get().is_null() {
            f.field("x_placement_device", &self.x_placement_device.get());
        }
        if !self.y_placement_device.get().is_null() {
            f.field("y_placement_device", &self.y_placement_device.get());
        }
        if !self.x_advance_device.get().is_null() {
            f.field("x_advance_device", &self.x_advance_device.get());
        }
        if !self.y_advance_device.get().is_null() {
            f.field("y_advance_device", &self.y_advance_device.get());
        }
        f.finish()
    }
}

impl ComputeSize for ValueRecord {
    #[inline]
    fn compute_size(args: &ValueFormat) -> Result<usize, ReadError> {
        Ok(args.record_byte_len())
    }
}

#[cfg(feature = "experimental_traverse")]
impl<'a> ValueRecord {
    pub(crate) fn traversal_type(&self, data: FontData<'a>) -> FieldType<'a> {
        FieldType::Record(self.clone().traverse(data))
    }

    pub(crate) fn get_field(&self, idx: usize, data: FontData<'a>) -> Option<Field<'a>> {
        let fields = [
            self.x_placement.is_some().then_some("x_placement"),
            self.y_placement.is_some().then_some("y_placement"),
            self.x_advance.is_some().then_some("x_advance"),
            self.y_advance.is_some().then_some("y_advance"),
            (!self.x_placement_device.get().is_null()).then_some("x_placement_device"),
            (!self.y_placement_device.get().is_null()).then_some("y_placement_device"),
            (!self.x_advance_device.get().is_null()).then_some("x_advance_device"),
            (!self.y_advance_device.get().is_null()).then_some("y_advance_device"),
        ];

        let name = fields.iter().filter_map(|x| *x).nth(idx)?;
        let typ: FieldType = match name {
            "x_placement" => self.x_placement().unwrap().into(),
            "y_placement" => self.y_placement().unwrap().into(),
            "x_advance" => self.x_advance().unwrap().into(),
            "y_advance" => self.y_advance().unwrap().into(),
            "x_placement_device" => {
                FieldType::offset(self.x_placement_device.get(), self.x_placement_device(data))
            }
            "y_placement_device" => {
                FieldType::offset(self.y_placement_device.get(), self.y_placement_device(data))
            }
            "x_advance_device" => {
                FieldType::offset(self.x_advance_device.get(), self.x_advance_device(data))
            }
            "y_advance_device" => {
                FieldType::offset(self.y_advance_device.get(), self.y_advance_device(data))
            }
            _ => panic!("hmm"),
        };

        Some(Field::new(name, typ))
    }
}

#[cfg(feature = "experimental_traverse")]
impl<'a> SomeRecord<'a> for ValueRecord {
    fn traverse(self, data: FontData<'a>) -> RecordResolver<'a> {
        RecordResolver {
            name: "ValueRecord",
            data,
            get_field: Box::new(move |idx, data| self.get_field(idx, data)),
        }
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
}
