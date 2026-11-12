use core;
#[cfg(feature="std")]
use std;
use ::alloc;
use super::interface::{c_void, CAllocator};
#[cfg(feature="std")]
use std::vec::Vec;
#[cfg(feature="std")]
pub use std::boxed::Box;

// The block is `ManuallyDrop`: blocks from a custom allocator must go back
// through `free_cell` (dropping them would hand them to the wrong allocator),
// so one that is dropped instead leaks, as before. Without a release-build
// `Drop` impl the decoder state has no drop glue to run on every block.
#[cfg(feature="std")]
pub struct MemoryBlock<Ty:Sized+Default> {
    inner: core::mem::ManuallyDrop<Box<[Ty]>>,
}
// Debug builds keep the upstream diagnostic for blocks leaked that way.
#[cfg(all(feature="std", debug_assertions))]
impl<Ty:Sized+Default> Drop for MemoryBlock<Ty> {
    fn drop(&mut self) {
        if self.inner.len() != 0 {
            print!("leaking memory block of length {} element size: {}\n", self.inner.len(), core::mem::size_of::<Ty>());
        }
    }
}
#[cfg(feature="std")]
#[allow(non_snake_case)]
#[inline(always)]
fn MemoryBlock<Ty:Sized+Default>(b: Box<[Ty]>) -> MemoryBlock<Ty> {
    MemoryBlock {
        inner: core::mem::ManuallyDrop::new(b),
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default> Default for MemoryBlock<Ty> {
    #[inline(always)]
    fn default() -> Self {
        MemoryBlock(Vec::<Ty>::new().into_boxed_slice())
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default> alloc::SliceWrapper<Ty> for MemoryBlock<Ty> {
    #[inline(always)]
    fn slice(&self) -> &[Ty] {
        &self.inner[..]
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default> alloc::SliceWrapperMut<Ty> for MemoryBlock<Ty> {
    #[inline(always)]
    fn slice_mut(&mut self) -> &mut [Ty] {
        &mut self.inner[..]
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default> core::ops::Index<usize> for MemoryBlock<Ty> {
    type Output = Ty;
    #[inline(always)]
    fn index(&self, index:usize) -> &Ty {
        &self.inner[index]
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default> core::ops::IndexMut<usize> for MemoryBlock<Ty> {
    #[inline(always)]
    fn index_mut(&mut self, index:usize) -> &mut Ty {
        &mut self.inner[index]
    }
}

// Fallible counterpart of vec![Ty::default(); size].into_boxed_slice(). vec!
// reports allocation failure through handle_alloc_error, which aborts the
// process (catch_unwind cannot intercept it), so a decoder created without C
// allocator callbacks crashed its host whenever a large allocation such as the
// ring buffer (up to 16 MiB, or 1 GiB with large windows) failed. Returning
// None lets callers report BROTLI_DECODER_ERROR_ALLOC_* exactly as they do
// when a C allocator returns null.
#[cfg(feature = "std")]
#[inline(never)]
fn try_alloc_default_slice<Ty: Sized + Default + Clone>(size: usize) -> Option<Box<[Ty]>> {
    let mut elements = if size == 0 || core::mem::size_of::<Ty>() == 0 {
        // Nothing to allocate: this Vec never touches the allocator.
        Vec::new()
    } else {
        let layout = match std::alloc::Layout::array::<Ty>(size) {
            Ok(layout) => layout,
            Err(_) => return None, // more than isize::MAX bytes
        };
        // Zeroed, as vec! requests for zero-valued elements, so that where the
        // optimizer can see the allocator (e.g. with LTO) the fill below folds
        // away and untouched pages stay uncommitted.
        let data = unsafe { std::alloc::alloc_zeroed(layout) } as *mut Ty;
        if data.is_null() {
            return None;
        }
        // SAFETY: data comes from the global allocator with the layout of
        // `size` elements of Ty, and length 0 exposes no uninitialized Ty.
        unsafe { Vec::from_raw_parts(data, 0, size) }
    };
    // Generic code cannot tell whether all-zero bytes are a valid Ty, so every
    // element is initialized. elements.len() counts the initialized prefix, so
    // if Ty::default() panics, dropping the Vec drops exactly those elements
    // and frees the buffer. The decoder's own element types (u8, u32 and
    // HuffmanCode) cannot panic here, since their Default returns a constant
    // zero; the tracking is for the other types this public impl serves, such
    // as the brotli crate's FFI, which reuses it for its encoder buffers.
    //
    // It is a plain loop rather than resize/resize_with because in this
    // crate's LTO builds only the loop's zero fill is optimized away after
    // alloc_zeroed; the others kept it, committing every page.
    for index in 0..size {
        unsafe {
            core::ptr::write(elements.as_mut_ptr().add(index), Ty::default());
            elements.set_len(index + 1);
        }
    }
    // The capacity already fits `size`, so this does not reallocate.
    Some(elements.into_boxed_slice())
}

pub struct SubclassableAllocator {
    alloc: CAllocator
    // have alternative ty here
}

impl SubclassableAllocator {
    pub unsafe fn new(sub_alloc:CAllocator) -> Self {
        SubclassableAllocator{
            alloc:sub_alloc,
        }
    }
}
#[cfg(feature="std")]
impl<Ty:Sized+Default+Clone> alloc::Allocator<Ty> for SubclassableAllocator {
    type AllocatedMemory = MemoryBlock<Ty>;
    fn alloc_cell(&mut self, size:usize) ->MemoryBlock<Ty>{
        if size == 0 {
            return MemoryBlock::<Ty>::default();
        }
        if let Some(alloc_fn) = self.alloc.alloc_func {
            let alloc_size = match size.checked_mul(core::mem::size_of::<Ty>()) {
                Some(alloc_size) => alloc_size,
                None => return MemoryBlock::<Ty>::default(),
            };
            let ptr = alloc_fn(self.alloc.opaque, alloc_size);
            if ptr.is_null() {
                return MemoryBlock::<Ty>::default();
            }
            let typed_ptr = unsafe {core::mem::transmute::<*mut c_void, *mut Ty>(ptr)};
            let slice_ref = unsafe {super::slice_from_raw_parts_or_nil_mut(typed_ptr, size)};
            for item in slice_ref.iter_mut() {
                unsafe{core::ptr::write(item, Ty::default())};
            }
            return MemoryBlock(unsafe{Box::from_raw(slice_ref)})
        }
        match try_alloc_default_slice(size) {
            Some(data) => MemoryBlock(data),
            None => MemoryBlock::<Ty>::default(),
        }
    }
    #[inline(always)]
    fn free_cell(&mut self, mut bv:MemoryBlock<Ty>) {
        if (*bv.inner).len() != 0 {
            if let Some(_) = self.alloc.alloc_func {
                let slice_ptr = (*bv.inner).as_mut_ptr();
                let _box_ptr = Box::into_raw(core::mem::replace(&mut *bv.inner, Vec::<Ty>::new().into_boxed_slice()));
                if let Some(free_fn) = self.alloc.free_func {
                    unsafe {free_fn(self.alloc.opaque, core::mem::transmute::<*mut Ty, *mut c_void>(slice_ptr))};
                }
            } else {
                let _to_free = core::mem::replace(&mut *bv.inner, Vec::<Ty>::new().into_boxed_slice());
            }
        }
    }
}











#[cfg(not(feature="std"))]
pub struct MemoryBlock<Ty:Sized+Default>(*mut[Ty]);
#[cfg(not(feature="std"))]
impl<Ty:Sized+Default> Default for MemoryBlock<Ty> {
    fn default() -> Self {
        // Even an empty slice reference needs a non-null, Ty-aligned pointer.
        MemoryBlock(core::ptr::slice_from_raw_parts_mut(
            core::ptr::NonNull::<Ty>::dangling().as_ptr(), 0))
    }
}
#[cfg(not(feature="std"))]
impl<Ty:Sized+Default> alloc::SliceWrapper<Ty> for MemoryBlock<Ty> {
    fn slice(&self) -> &[Ty] {
        // The pointer is either an aligned empty slice or an initialized
        // allocation from alloc_cell; the borrow cannot outlive this block.
        unsafe { &*self.0 }
    }
}
#[cfg(not(feature="std"))]
impl<Ty:Sized+Default> alloc::SliceWrapperMut<Ty> for MemoryBlock<Ty> {
    fn slice_mut(&mut self) -> &mut [Ty] {
        // As above, with exclusive access guaranteed by the mutable borrow.
        unsafe { &mut *self.0 }
    }
}

#[cfg(not(feature="std"))]
#[cfg(feature="no-stdlib-ffi-binding")]
#[panic_handler]
extern fn panic_impl(_: &::core::panic::PanicInfo) -> ! {
    loop {}
}
#[cfg(not(feature="std"))]
#[cfg(feature="no-stdlib-ffi-binding")]
#[lang = "eh_personality"]
extern "C" fn eh_personality() {
}

#[cfg(not(feature="std"))]
impl<Ty:Sized+Default> core::ops::Index<usize> for MemoryBlock<Ty> {
    type Output = Ty;
    fn index(&self, index:usize) -> &Ty {
        &alloc::SliceWrapper::slice(self)[index]
    }
}
#[cfg(not(feature="std"))]
impl<Ty:Sized+Default> core::ops::IndexMut<usize> for MemoryBlock<Ty> {

    fn index_mut(&mut self, index:usize) -> &mut Ty {
        &mut alloc::SliceWrapperMut::slice_mut(self)[index]
    }
}

#[cfg(not(feature="std"))]
impl<Ty:Sized+Default+Clone> alloc::Allocator<Ty> for SubclassableAllocator {
    type AllocatedMemory = MemoryBlock<Ty>;
    fn alloc_cell(&mut self, size:usize) ->MemoryBlock<Ty>{
        if size == 0 {
            return MemoryBlock::<Ty>::default();
        }
        if let Some(alloc_fn) = self.alloc.alloc_func {
            let alloc_size = match size.checked_mul(core::mem::size_of::<Ty>()) {
                Some(alloc_size) => alloc_size,
                None => return MemoryBlock::<Ty>::default(),
            };
            let ptr = alloc_fn(self.alloc.opaque, alloc_size);
            if ptr.is_null() {
                return MemoryBlock::<Ty>::default();
            }
            let typed_ptr = unsafe {core::mem::transmute::<*mut c_void, *mut Ty>(ptr)};
            let slice_ref = unsafe {super::slice_from_raw_parts_or_nil_mut(typed_ptr, size)};
            for item in slice_ref.iter_mut() {
                unsafe{core::ptr::write(item, Ty::default())};
            }
            return MemoryBlock(slice_ref)
        } else {
            panic!("Must provide allocators in no-stdlib code");
        }
    }
    fn free_cell(&mut self, mut bv:MemoryBlock<Ty>) {
        use alloc::SliceWrapper;
        use alloc::SliceWrapperMut;
        if bv.slice().len() != 0 {
            if let Some(_) = self.alloc.alloc_func {
                if let Some(free_fn) = self.alloc.free_func {
                    unsafe {free_fn(self.alloc.opaque, core::mem::transmute::<*mut Ty, *mut c_void>(&mut bv.slice_mut()[0]))};
                }
                let _ = core::mem::replace(&mut bv,
                                           MemoryBlock::<Ty>::default());
            } else {
                panic!("Must provide allocators in no-stdlib code");
            }
        }
    }
}


#[cfg(not(feature="std"))]
pub fn free_stdlib<T>(_data: *mut T, _size: usize) {
    panic!("Must supply allocators if calling divans when compiled with features=no-stdlib");
}
#[cfg(not(feature="std"))]
pub fn alloc_stdlib<T:Sized+Default+Copy+Clone>(_size: usize) -> *mut T {
    panic!("Must supply allocators if calling divans when compiled with features=no-stdlib");
}

#[cfg(feature="std")]
pub unsafe fn free_stdlib<T>(ptr: *mut T, size: usize) {
    if ptr.is_null() {
        return;
    }
    let slice_ref = super::slice_from_raw_parts_or_nil_mut(ptr, size);
    let _ = Box::from_raw(slice_ref); // free on drop
}
#[cfg(feature="std")]
pub fn alloc_stdlib<T:Sized+Default+Copy+Clone>(size: usize) -> *mut T {
    match try_alloc_default_slice::<T>(size) {
        Some(newly_allocated) => Box::into_raw(newly_allocated) as *mut T,
        None => core::ptr::null_mut(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ::alloc::{Allocator, SliceWrapper, SliceWrapperMut};

    #[repr(align(64))]
    #[derive(Clone, Default)]
    struct OverAligned {
        _byte: u8,
    }

    fn assert_empty_block<Ty: Sized + Default>(mut block: MemoryBlock<Ty>) {
        #[cfg(not(feature="std"))]
        {
            // Inspect the raw pointer first, so a regression fails without
            // creating an invalid reference just to check the slice length.
            let data = block.0 as *mut Ty;
            assert!(!data.is_null());
            assert_eq!(data as usize % core::mem::align_of::<Ty>(), 0);
        }
        assert!(block.slice().is_empty());
        assert_eq!(block.slice().as_ptr() as usize % core::mem::align_of::<Ty>(), 0);
        let slice = block.slice_mut();
        assert!(slice.is_empty());
        assert_eq!(slice.as_mut_ptr() as usize % core::mem::align_of::<Ty>(), 0);
    }

    #[test]
    fn default_empty_blocks_are_aligned_for_their_element_type() {
        assert_empty_block(MemoryBlock::<u8>::default());
        assert_empty_block(MemoryBlock::<u32>::default());
        assert_empty_block(MemoryBlock::<super::super::HuffmanCode>::default());
        assert_empty_block(MemoryBlock::<OverAligned>::default());
    }

    #[cfg(not(feature="std"))]
    #[test]
    fn nonempty_block_supports_slices_and_indexing() {
        let mut data = [1u32, 2, 3];
        {
            let mut block = MemoryBlock(&mut data[..] as *mut [u32]);
            assert_eq!(block.slice(), &[1, 2, 3]);
            assert_eq!(block[1], 2);
            block[1] = 4;
            block.slice_mut()[2] = 5;
            assert_eq!(block.slice(), &[1, 4, 5]);
        }
        assert_eq!(data, [1, 4, 5]);
    }

    extern "C" fn failing_alloc(_data: *mut c_void, _size: usize) -> *mut c_void {
        core::ptr::null_mut()
    }

    #[test]
    fn failed_custom_allocation_returns_empty_block() {
        let c_allocator = CAllocator {
            alloc_func: Some(failing_alloc),
            free_func: None,
            opaque: core::ptr::null_mut(),
        };
        let mut allocator = unsafe { SubclassableAllocator::new(c_allocator) };
        let block =
            <SubclassableAllocator as Allocator<u8>>::alloc_cell(&mut allocator, 1);

        assert_eq!(block.slice().len(), 0);
    }

    #[test]
    fn zero_and_failed_allocations_return_aligned_empty_blocks() {
        let mut allocator = unsafe {
            SubclassableAllocator::new(CAllocator {
                alloc_func: Some(failing_alloc),
                free_func: None,
                opaque: core::ptr::null_mut(),
            })
        };
        for &size in &[0usize, 1, usize::MAX] {
            let block = <SubclassableAllocator as Allocator<OverAligned>>::alloc_cell(
                &mut allocator, size);
            assert_empty_block(block);
        }
    }

    // Without C callbacks alloc_cell used vec!, which panics ("capacity
    // overflow") on sizes it cannot represent and aborts when the allocation
    // itself fails, instead of returning the empty block callers check for.
    #[cfg(feature = "std")]
    #[test]
    fn default_allocator_failures_return_empty_blocks() {
        let mut allocator = unsafe {
            SubclassableAllocator::new(CAllocator {
                alloc_func: None,
                free_func: None,
                opaque: core::ptr::null_mut(),
            })
        };
        assert_empty_block(<SubclassableAllocator as Allocator<u8>>::alloc_cell(
            &mut allocator,
            isize::MAX as usize + 1,
        ));
        assert_empty_block(<SubclassableAllocator as Allocator<u32>>::alloc_cell(
            &mut allocator,
            usize::MAX,
        ));
        assert_empty_block(
            <SubclassableAllocator as Allocator<OverAligned>>::alloc_cell(
                &mut allocator,
                usize::MAX,
            ),
        );
        assert!(alloc_stdlib::<u8>(isize::MAX as usize + 1).is_null());

        let block = <SubclassableAllocator as Allocator<u32>>::alloc_cell(&mut allocator, 5);
        assert_eq!(block.slice(), &[0u32; 5]);
        <SubclassableAllocator as Allocator<u32>>::free_cell(&mut allocator, block);
        let block =
            <SubclassableAllocator as Allocator<OverAligned>>::alloc_cell(&mut allocator, 3);
        assert_eq!(block.slice().len(), 3);
        assert_eq!(
            block.slice().as_ptr() as usize % core::mem::align_of::<OverAligned>(),
            0
        );
        <SubclassableAllocator as Allocator<OverAligned>>::free_cell(&mut allocator, block);
    }

    // Default and Clone both make a new value, and the third one panics.
    #[cfg(feature = "std")]
    mod panicking_element {
        use super::*;
        use std::cell::Cell;

        thread_local! {
            static MADE: Cell<usize> = Cell::new(0);
            static DROPS: Cell<usize> = Cell::new(0);
        }

        struct PanicsOnThirdValue {
            _value: u32,
        }

        impl PanicsOnThirdValue {
            fn new() -> Self {
                let made = MADE.with(|count| count.replace(count.get() + 1));
                assert!(made < 2, "third value");
                PanicsOnThirdValue { _value: 7 }
            }
        }

        impl Default for PanicsOnThirdValue {
            fn default() -> Self {
                PanicsOnThirdValue::new()
            }
        }

        impl Clone for PanicsOnThirdValue {
            fn clone(&self) -> Self {
                PanicsOnThirdValue::new()
            }
        }

        impl Drop for PanicsOnThirdValue {
            fn drop(&mut self) {
                DROPS.with(|count| count.set(count.get() + 1));
            }
        }

        fn alloc_five() -> MemoryBlock<PanicsOnThirdValue> {
            let mut allocator = unsafe {
                SubclassableAllocator::new(CAllocator {
                    alloc_func: None,
                    free_func: None,
                    opaque: core::ptr::null_mut(),
                })
            };
            <SubclassableAllocator as Allocator<PanicsOnThirdValue>>::alloc_cell(&mut allocator, 5)
        }

        // The panic reaches the caller instead of becoming an allocation
        // failure. This also runs under panic=abort, where the panic ends the
        // process that the test harness starts for this test.
        #[test]
        #[should_panic(expected = "third value")]
        fn default_allocator_propagates_the_panic() {
            alloc_five();
        }

        // With unwinding, the values made before the panic must not leak; vec!,
        // which alloc_cell used before, drops them too. Under panic=abort
        // nothing unwinds, so there is nothing to drop.
        #[cfg(panic = "unwind")]
        #[test]
        fn default_allocator_drops_the_values_made_before_the_panic() {
            assert!(std::panic::catch_unwind(alloc_five).is_err());
            assert_eq!(MADE.with(|count| count.get()), 3);
            assert_eq!(DROPS.with(|count| count.get()), 2);
        }
    }
}
