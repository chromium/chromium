//! Scratch space for loading an outline, and how much to supply.

use super::{
    super::{Point, PointFlags},
    scale::Scale,
    OutlineError,
};
use crate::mem::take_slice;
use bytemuck::{AnyBitPattern, NoUninit};

/// Scratch space for [`OutlinePlan::load`](super::OutlinePlan::load).
///
/// Every buffer comes from the caller. Size them with
/// [`OutlinePlan::buffer_lengths`](super::OutlinePlan::buffer_lengths); a buffer
/// shorter than that is reported as [`OutlineError::InsufficientMemory`], not
/// overrun.
///
/// [`OutlineError::InsufficientMemory`]: super::OutlineError::InsufficientMemory
pub struct Buffers<'a, S: Scale> {
    /// Receives the loaded points.
    pub points: &'a mut [Point<S::Coord>],
    /// Receives the point flags.
    pub flags: &'a mut [PointFlags],
    /// Receives the contour end point indices.
    pub contours: &'a mut [u16],
    /// Font unit coordinates. Read only when [`Scale::NEEDS_UNSCALED`],
    /// and may be empty otherwise.
    pub unscaled: &'a mut [Point<i32>],
    /// Variation delta scratch. May be empty for a static instance.
    pub deltas: &'a mut [Point<S::Delta>],
    /// Delta interpolation scratch. May be empty for a static instance.
    pub iup: &'a mut [Point<S::Delta>],
    /// Per-component delta stack. May be empty for a static instance.
    pub composite_deltas: &'a mut [Point<S::Delta>],
}

/// How long each scratch buffer an outline needs has to be.
///
/// These are element counts, not byte lengths: the element types differ per
/// buffer and, for the coordinate buffers, per [`Scale`]. For the bytes one
/// block needs to hold them all, see [`packed_len`][Self::packed_len].
///
/// Created by [`OutlinePlan::buffer_lengths`](super::OutlinePlan::buffer_lengths).
#[derive(Copy, Clone, Default, PartialEq, Eq, Debug)]
pub struct BufferLengths {
    /// Scaled points, including phantom points. The flag buffer is this long
    /// too, since there is one flag per point.
    pub points: u32,
    /// Contour end point indices.
    pub contours: u32,
    /// Points in font units, or zero when the [`Scale`] does not keep them.
    pub unscaled: u32,
    /// Accumulated variation deltas, or zero for a non-variable draw.
    pub deltas: u32,
    /// Working space for interpolating sparse deltas.
    pub iup: u32,
    /// Per-component deltas for composite glyphs.
    pub composite_deltas: u32,
}

impl BufferLengths {
    /// The number of bytes [`Buffers::from_bytes`] needs to carve every
    /// buffer from one block.
    ///
    /// Includes room to align the block, which need not be aligned already.
    /// Zero when the outline needs no storage.
    pub fn packed_len<S: Scale>(&self) -> usize {
        let coord = size_of::<Point<S::Coord>>();
        let delta = size_of::<Point<S::Delta>>();
        let size = self.points as usize * (coord + size_of::<PointFlags>())
            + self.contours as usize * size_of::<u16>()
            + self.unscaled as usize * size_of::<Point<i32>>()
            + (self.deltas + self.iup + self.composite_deltas) as usize * delta;
        if size == 0 {
            return 0;
        }
        // Buffers are carved out in descending order of alignment, so only the
        // first one can need padding, and one alignment's worth covers it.
        size + align_of::<Point<S::Coord>>()
            .max(align_of::<Point<S::Delta>>())
            .max(align_of::<Point<i32>>())
    }
}

impl<'a, S: Scale> Buffers<'a, S> {
    /// Carves all seven buffers out of one block of bytes.
    ///
    /// For a caller with its own allocator, or one that prefers a single
    /// allocation per draw. `bytes` must be at least
    /// [`BufferLengths::packed_len`] long and need not be aligned.
    ///
    /// ```
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// use read_fonts::{
    ///     tables::glyf::outline::{
    ///         Buffers, OutlinePlan, OutlineTables, Scale26Dot6,
    ///     },
    ///     types::GlyphId,
    ///     FontRef,
    /// };
    ///
    /// let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE)?;
    /// let tables = OutlineTables::new(&font)?;
    /// let plan = OutlinePlan::new(&tables, GlyphId::new(1))?;
    ///
    /// let lengths = plan.buffer_lengths::<Scale26Dot6>();
    /// let mut block = vec![0u8; lengths.packed_len::<Scale26Dot6>()];
    /// let buffers = Buffers::from_bytes(&mut block, &lengths)?;
    ///
    /// let scale = Scale26Dot6::new(Some(16.0), tables.units_per_em);
    /// let outline = plan.load(&tables, &scale, buffers, None)?;
    /// assert_eq!(outline.points().len(), outline.flags().len());
    /// # Ok(())
    /// # }
    /// ```
    pub fn from_bytes(bytes: &'a mut [u8], lengths: &BufferLengths) -> Result<Self, OutlineError> {
        // Descending alignment, so only the first allocation can need padding.
        let (points, bytes) = take(bytes, lengths.points)?;
        let (unscaled, bytes) = take(bytes, lengths.unscaled)?;
        let (deltas, bytes) = take(bytes, lengths.deltas)?;
        let (iup, bytes) = take(bytes, lengths.iup)?;
        let (composite_deltas, bytes) = take(bytes, lengths.composite_deltas)?;
        let (contours, bytes) = take(bytes, lengths.contours)?;
        let (flags, _) = take(bytes, lengths.points)?;
        Ok(Self {
            points,
            flags,
            contours,
            unscaled,
            deltas,
            iup,
            composite_deltas,
        })
    }
}

/// Takes a `len` element slice of `T` off the front of `bytes`, in the terms
/// an outline reports.
fn take<T>(bytes: &mut [u8], len: u32) -> Result<(&mut [T], &mut [u8]), OutlineError>
where
    T: AnyBitPattern + NoUninit,
{
    take_slice(bytes, len as usize).ok_or(OutlineError::InsufficientMemory)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        tables::glyf::outline::{
            Outline, OutlinePlan, OutlineTables, Scale26Dot6, ScaleF32, Unscaled,
        },
        types::{F26Dot6, F2Dot14, GlyphId},
        FontRef, TableProvider,
    };
    use alloc::vec;

    /// An outline built in one block of bytes matches one built in separate
    /// vectors, for every glyph of a font and at every scale.
    #[test]
    fn packed_buffers_load_the_same_outline() {
        fn check<S: Scale>(scale: &S)
        where
            S::Coord: core::fmt::Debug,
        {
            let font = FontRef::new(font_test_data::COLRV0V1_VARIABLE).unwrap();
            let tables = OutlineTables::new(&font).unwrap();
            for gid in 0..font.maxp().unwrap().num_glyphs() {
                let gid = GlyphId::from(gid);
                let plan = OutlinePlan::new(&tables, gid).unwrap();
                let lengths = plan.buffer_lengths::<S>();

                let mut owned = Outline::<S>::new();
                let expected = owned.load_with(&tables, gid, scale, None).unwrap();
                let expected = (expected.points().to_vec(), expected.contours().to_vec());

                let mut block = vec![0u8; lengths.packed_len::<S>()];
                let buffers = Buffers::from_bytes(&mut block, &lengths).unwrap();
                let actual = plan.load(&tables, scale, buffers, None).unwrap();
                assert_eq!(actual.points(), expected.0, "gid {gid}");
                assert_eq!(actual.contours(), expected.1, "gid {gid}");
            }
        }
        check(&Scale26Dot6::new(Some(16.0), 1000));
        check(&ScaleF32::new(Some(16.0), 1000));
        check(&Unscaled);
    }

    /// The reported size is enough, and one byte less is not.
    #[test]
    fn the_reported_size_is_what_it_takes() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let tables = OutlineTables::new(&font).unwrap();
        let plan = OutlinePlan::new(&tables, GlyphId::new(5)).unwrap();
        let lengths = plan.buffer_lengths::<Scale26Dot6>();
        let size = lengths.packed_len::<Scale26Dot6>();
        assert!(size > 0);

        let mut block = vec![0u8; size];
        assert!(Buffers::<Scale26Dot6>::from_bytes(&mut block, &lengths).is_ok());

        // The slack is for alignment, so a short block only has to fail when
        // it is short by more than that slack.
        let mut short = vec![0u8; size - core::mem::align_of::<Point<F26Dot6>>() - 1];
        assert_eq!(
            Buffers::<Scale26Dot6>::from_bytes(&mut short, &lengths).err(),
            Some(OutlineError::InsufficientMemory)
        );
    }

    /// A block that does not begin on an alignment boundary still works.
    #[test]
    fn an_unaligned_block_is_fine() {
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();
        let tables = OutlineTables::new(&font).unwrap();
        let plan = OutlinePlan::new(&tables, GlyphId::new(5)).unwrap();
        let lengths = plan.buffer_lengths::<Scale26Dot6>();
        let scale = Scale26Dot6::new(Some(16.0), 1000);

        let aligned = {
            let mut block = vec![0u8; lengths.packed_len::<Scale26Dot6>()];
            let buffers = Buffers::from_bytes(&mut block, &lengths).unwrap();
            plan.load(&tables, &scale, buffers, None)
                .unwrap()
                .points()
                .to_vec()
        };
        for offset in 1..8 {
            let mut block = vec![0u8; lengths.packed_len::<Scale26Dot6>() + offset];
            let buffers = Buffers::from_bytes(&mut block[offset..], &lengths).unwrap();
            let outline = plan.load(&tables, &scale, buffers, None).unwrap();
            assert_eq!(outline.points(), aligned, "offset {offset}");
        }
    }

    /// Every carved buffer is at least as long as its count asks for, and
    /// correctly aligned.
    #[test]
    fn every_buffer_is_sized_and_aligned() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        // At a location, so the delta buffers are non-empty too.
        let coords = [F2Dot14::from_f32(1.0)];
        let tables = OutlineTables::new(&font).unwrap().at(&coords, &[]);
        let plan = OutlinePlan::new(&tables, GlyphId::new(2)).unwrap();
        assert!(plan.applies_variations);
        let lengths = plan.buffer_lengths::<Scale26Dot6>();
        let mut block = vec![0u8; lengths.packed_len::<Scale26Dot6>()];
        let buffers = Buffers::<Scale26Dot6>::from_bytes(&mut block, &lengths).unwrap();
        assert_eq!(buffers.points.len(), lengths.points as usize);
        assert_eq!(buffers.flags.len(), lengths.points as usize);
        assert_eq!(buffers.contours.len(), lengths.contours as usize);
        assert_eq!(buffers.unscaled.len(), lengths.unscaled as usize);
        assert_eq!(buffers.deltas.len(), lengths.deltas as usize);
        assert_eq!(buffers.iup.len(), lengths.iup as usize);
        assert_eq!(
            buffers.composite_deltas.len(),
            lengths.composite_deltas as usize
        );
        assert!(lengths.deltas > 0, "expected a varying glyph");
        // Casting through bytemuck would have failed on a misaligned slice,
        // but check the pointers rather than trust that.
        assert_eq!(buffers.points.as_ptr().align_offset(4), 0);
        assert_eq!(buffers.contours.as_ptr().align_offset(2), 0);
    }

    /// An empty glyph needs no storage at all.
    #[test]
    fn an_empty_outline_needs_no_bytes() {
        let lengths = BufferLengths::default();
        assert_eq!(lengths.packed_len::<Scale26Dot6>(), 0);
        let buffers = Buffers::<Scale26Dot6>::from_bytes(&mut [], &lengths).unwrap();
        assert!(buffers.points.is_empty());
        assert!(buffers.flags.is_empty());
    }
}
