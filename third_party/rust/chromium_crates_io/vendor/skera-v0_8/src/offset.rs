//! Handling offsets

use crate::serialize::{OffsetWhence, SerializeErrorFlags, Serializer};
use crate::{Plan, Serialize, SubsetTable};
use write_fonts::{
    read::MinByteRange,
    types::{FixedSize, Scalar},
};

pub(crate) trait SerializeSubset {
    fn serialize_subset<'a, T: SubsetTable<'a>>(
        t: &T,
        s: &mut Serializer,
        plan: &Plan,
        args: T::ArgsForSubset,
        pos: usize,
    ) -> Result<T::Output, SerializeErrorFlags>;
}

impl<O: Scalar> SerializeSubset for O {
    fn serialize_subset<'a, T: SubsetTable<'a>>(
        t: &T,
        s: &mut Serializer,
        plan: &Plan,
        args: T::ArgsForSubset,
        pos: usize,
    ) -> Result<T::Output, SerializeErrorFlags> {
        s.push()?;
        match t.subset(plan, s, args) {
            Ok(ret) => {
                let Some(obj_idx) = s.pop_pack(true) else {
                    return Err(s.error());
                };
                s.add_link(
                    pos..pos + O::RAW_BYTE_LEN,
                    obj_idx,
                    OffsetWhence::Head,
                    0,
                    false,
                )?;
                Ok(ret)
            }
            Err(e) => {
                s.pop_discard();
                Err(e)
            }
        }
    }
}

// this is trait is used to copy simple tables only which implemented MinByteRange trait
pub(crate) trait SerializeCopy {
    fn serialize_copy<'a, T: MinByteRange<'a>>(
        t: &T,
        s: &mut Serializer,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags>;

    fn serialize_copy_from_bytes(
        src_bytes: &[u8],
        s: &mut Serializer,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags>;
}

impl<O: Scalar> SerializeCopy for O {
    fn serialize_copy<'a, T: MinByteRange<'a>>(
        t: &T,
        s: &mut Serializer,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags> {
        s.push()?;
        s.embed_bytes(t.min_table_bytes())?;

        let Some(obj_idx) = s.pop_pack(true) else {
            return Err(s.error());
        };
        s.add_link(
            pos..pos + O::RAW_BYTE_LEN,
            obj_idx,
            OffsetWhence::Head,
            0,
            false,
        )
    }

    fn serialize_copy_from_bytes(
        src_bytes: &[u8],
        s: &mut Serializer,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags> {
        s.push()?;
        s.embed_bytes(src_bytes)?;

        let Some(obj_idx) = s.pop_pack(true) else {
            return Err(s.error());
        };
        s.add_link(
            pos..pos + O::RAW_BYTE_LEN,
            obj_idx,
            OffsetWhence::Head,
            0,
            false,
        )
    }
}

/// this is trait is used to serialize a table that an offset at pos points to
pub(crate) trait SerializeSerialize {
    fn serialize_serialize<'a, T: Serialize<'a>>(
        s: &mut Serializer,
        args: T::Args,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags>;
}

impl<O: Scalar> SerializeSerialize for O {
    fn serialize_serialize<'a, T: Serialize<'a>>(
        s: &mut Serializer,
        args: T::Args,
        pos: usize,
    ) -> Result<(), SerializeErrorFlags> {
        s.push()?;
        match T::serialize(s, args) {
            Ok(()) => {
                let Some(obj_idx) = s.pop_pack(true) else {
                    return Err(s.error());
                };
                s.add_link(
                    pos..pos + O::RAW_BYTE_LEN,
                    obj_idx,
                    OffsetWhence::Head,
                    0,
                    false,
                )?;
                Ok(())
            }
            Err(e) => {
                s.pop_discard();
                Err(e)
            }
        }
    }
}
