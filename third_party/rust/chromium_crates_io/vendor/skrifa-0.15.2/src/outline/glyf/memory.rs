//! Memory allocation for TrueType scaling.

use std::mem::{align_of, size_of};

use read_fonts::{
    tables::glyf::PointFlags,
    types::{F26Dot6, Fixed, Point},
};

use super::{super::Hinting, Outline};

/// Buffers used during glyph scaling.
pub struct OutlineMemory<'a> {
    pub unscaled: &'a mut [Point<i32>],
    pub scaled: &'a mut [Point<F26Dot6>],
    pub original_scaled: &'a mut [Point<F26Dot6>],
    pub contours: &'a mut [u16],
    pub flags: &'a mut [PointFlags],
    pub deltas: &'a mut [Point<Fixed>],
    pub iup_buffer: &'a mut [Point<Fixed>],
    pub composite_deltas: &'a mut [Point<Fixed>],
}

impl<'a> OutlineMemory<'a> {
    pub(super) fn new(outline: &Outline, buf: &'a mut [u8], hinting: Hinting) -> Option<Self> {
        let (scaled, buf) = alloc_slice(buf, outline.points)?;
        let (unscaled, buf) = alloc_slice(buf, outline.max_other_points)?;
        // We only need original scaled points when hinting
        let (original_scaled, buf) = if outline.has_hinting && hinting == Hinting::Embedded {
            alloc_slice(buf, outline.max_other_points)?
        } else {
            (Default::default(), buf)
        };
        // Don't allocate any delta buffers if we don't have variations
        let (deltas, iup_buffer, composite_deltas, buf) = if outline.has_variations {
            let (deltas, buf) = alloc_slice(buf, outline.max_simple_points)?;
            let (iup_buffer, buf) = alloc_slice(buf, outline.max_simple_points)?;
            let (composite_deltas, buf) = alloc_slice(buf, outline.max_component_delta_stack)?;
            (deltas, iup_buffer, composite_deltas, buf)
        } else {
            (
                Default::default(),
                Default::default(),
                Default::default(),
                buf,
            )
        };
        let (contours, buf) = alloc_slice(buf, outline.contours)?;
        let (flags, _) = alloc_slice(buf, outline.points)?;
        Some(Self {
            unscaled,
            scaled,
            original_scaled,
            contours,
            flags,
            deltas,
            iup_buffer,
            composite_deltas,
        })
    }
}

/// Trait that defines which types may be used as element types for
/// `alloc_slice`.
///
/// # Safety
/// This must only be implemented for types that contain no internal padding
/// and for which all bit patterns are valid.
unsafe trait TransmuteElement: Sized + Copy {}

unsafe impl TransmuteElement for u16 {}
unsafe impl TransmuteElement for i32 {}
unsafe impl TransmuteElement for Fixed {}
unsafe impl TransmuteElement for F26Dot6 {}
unsafe impl TransmuteElement for PointFlags {}
unsafe impl<T> TransmuteElement for Point<T> where T: TransmuteElement {}

/// Allocates a mutable slice of `T` of the given length from the specified
/// buffer.
///
/// Returns the allocated slice and the remainder of the buffer.
fn alloc_slice<T: TransmuteElement>(buf: &mut [u8], len: usize) -> Option<(&mut [T], &mut [u8])> {
    if len == 0 {
        return Some((Default::default(), buf));
    }
    // 1) Ensure we slice the buffer at a position that is properly aligned
    // for T.
    let base_ptr = buf.as_ptr() as usize;
    let aligned_ptr = align_up(base_ptr, align_of::<T>());
    let aligned_offset = aligned_ptr - base_ptr;
    let buf = buf.get_mut(aligned_offset..)?;
    // 2) Ensure we have enough space in the buffer to allocate our slice.
    let len_in_bytes = len * size_of::<T>();
    if len_in_bytes > buf.len() {
        return None;
    }
    let (slice_buf, rest) = buf.split_at_mut(len_in_bytes);
    // SAFETY: Alignment and size requirements have been checked in 1) and
    // 2) above, respectively.
    // Element types are limited by the `TransmuteElement` trait which
    // defines requirements for transmutation and is private to this module.
    let slice = unsafe { std::slice::from_raw_parts_mut(slice_buf.as_mut_ptr() as *mut T, len) };
    Some((slice, rest))
}

fn align_up(len: usize, alignment: usize) -> usize {
    len + (len.wrapping_neg() & (alignment - 1))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unaligned_buffer() {
        let mut buf = [0u8; 40];
        let alignment = align_of::<i32>();
        let addr = buf.as_ptr() as usize;
        let mut unaligned_addr = addr;
        // Force an unaligned offset
        if unaligned_addr % alignment == 0 {
            unaligned_addr += 1;
        }
        let unaligned_offset = unaligned_addr - addr;
        let unaligned = &mut buf[unaligned_offset..];
        assert!(unaligned.as_ptr() as usize % alignment != 0);
        let (slice, _) = alloc_slice::<i32>(unaligned, 8).unwrap();
        assert_eq!(slice.as_ptr() as usize % alignment, 0);
    }

    #[test]
    fn fail_unaligned_buffer() {
        let mut buf = [0u8; 40];
        let alignment = align_of::<i32>();
        let addr = buf.as_ptr() as usize;
        let mut unaligned_addr = addr;
        // Force an unaligned offset
        if unaligned_addr % alignment == 0 {
            unaligned_addr += 1;
        }
        let unaligned_offset = unaligned_addr - addr;
        let unaligned = &mut buf[unaligned_offset..];
        assert_eq!(alloc_slice::<i32>(unaligned, 16), None);
    }

    #[test]
    fn outline_memory() {
        let outline_info = Outline {
            glyph: None,
            glyph_id: Default::default(),
            points: 10,
            contours: 4,
            max_simple_points: 4,
            max_other_points: 4,
            max_component_delta_stack: 4,
            has_hinting: false,
            has_variations: true,
            has_overlaps: false,
        };
        let required_size = outline_info.required_buffer_size(Hinting::None);
        let mut buf = vec![0u8; required_size];
        let memory = OutlineMemory::new(&outline_info, &mut buf, Hinting::None).unwrap();
        assert_eq!(memory.scaled.len(), outline_info.points);
        assert_eq!(memory.unscaled.len(), outline_info.max_other_points);
        // We don't allocate this buffer when hinting is disabled
        assert_eq!(memory.original_scaled.len(), 0);
        assert_eq!(memory.flags.len(), outline_info.points);
        assert_eq!(memory.contours.len(), outline_info.contours);
        assert_eq!(memory.deltas.len(), outline_info.max_simple_points);
        assert_eq!(memory.iup_buffer.len(), outline_info.max_simple_points);
        assert_eq!(
            memory.composite_deltas.len(),
            outline_info.max_component_delta_stack
        );
    }

    #[test]
    fn fail_outline_memory() {
        let outline_info = Outline {
            glyph: None,
            glyph_id: Default::default(),
            points: 10,
            contours: 4,
            max_simple_points: 4,
            max_other_points: 4,
            max_component_delta_stack: 4,
            has_hinting: false,
            has_variations: true,
            has_overlaps: false,
        };
        // Required size adds 4 bytes slop to account for internal alignment
        // requirements. So subtract 5 to force a failure.
        let not_enough = outline_info.required_buffer_size(Hinting::None) - 5;
        let mut buf = vec![0u8; not_enough];
        assert!(OutlineMemory::new(&outline_info, &mut buf, Hinting::None).is_none());
    }
}
