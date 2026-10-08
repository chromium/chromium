//! Subset arrays of offsets
use crate::{offset::SerializeSubset, Plan, SerializeErrorFlags, Serializer, SubsetTable};
use write_fonts::{
    read::{ArrayOfNullableOffsets, ArrayOfOffsets, FontRead, Offset, ReadError},
    types::{FixedSize, Scalar},
};

pub(crate) trait SubsetOffsetArray<'a, T: SubsetTable<'a>> {
    fn subset_offset(
        &self,
        idx: usize,
        s: &mut Serializer,
        plan: &Plan,
        args: T::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags>;
}

impl<'a, T, O> SubsetOffsetArray<'a, T> for ArrayOfOffsets<'a, T, O>
where
    O: Scalar + Offset + FixedSize + SerializeSubset,
    T: FontRead<'a> + SubsetTable<'a>,
    T::Args: Copy + 'static,
{
    fn subset_offset(
        &self,
        idx: usize,
        s: &mut Serializer,
        plan: &Plan,
        args: T::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let t = match self.get(idx) {
            Err(ReadError::NullOffset) => return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY),
            Ok(table) => table,
            Err(_) => return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)),
        };
        let snap = s.snapshot();
        let offset_pos = s.allocate_size(O::RAW_BYTE_LEN, true)?;

        if let Err(e) = O::serialize_subset(&t, s, plan, args, offset_pos) {
            s.revert_snapshot(snap);
            return Err(e);
        }

        Ok(())
    }
}

impl<'a, T, O> SubsetOffsetArray<'a, T> for ArrayOfNullableOffsets<'a, T, O>
where
    O: Scalar + Offset + FixedSize + SerializeSubset,
    T: FontRead<'a> + SubsetTable<'a>,
    T::Args: Copy + 'static,
{
    fn subset_offset(
        &self,
        idx: usize,
        s: &mut Serializer,
        plan: &Plan,
        args: T::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        match self.get(idx) {
            Some(Ok(t)) => {
                let snap = s.snapshot();
                let offset_pos = s.allocate_size(O::RAW_BYTE_LEN, true)?;

                match O::serialize_subset(&t, s, plan, args, offset_pos) {
                    Ok(_) => Ok(()),
                    Err(e) => {
                        s.revert_snapshot(snap);
                        Err(e)
                    }
                }
            }
            None => Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY),
            Some(Err(_)) => Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)),
        }
    }
}

pub(crate) trait IterNullableHelper<'a, T> {
    fn iter_as_nullable(&self) -> impl Iterator<Item = Option<Result<T, ReadError>>> + 'a;
}

impl<'a, T, O> IterNullableHelper<'a, T> for ArrayOfOffsets<'a, T, O>
// these bounds have to match what is in the read-fonts impl block that has the normal `iter` method
where
    O: Scalar + Offset,
    T: FontRead<'a>,
    T::Args: Copy + 'static,
{
    fn iter_as_nullable(&self) -> impl Iterator<Item = Option<Result<T, ReadError>>> + 'a {
        self.iter().map(|off| match off {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        })
    }
}
