use crate::rust_slice::RustSlice;
use core::mem::MaybeUninit;
use core::ptr::{self, NonNull};

#[unsafe(export_name = "cxxbridge1$slice$new")]
unsafe extern "C" fn slice_new(this: &mut MaybeUninit<RustSlice>, ptr: NonNull<()>, len: usize) {
    let this = this.as_mut_ptr();
    let rust_slice = RustSlice::from_raw_parts(ptr, len);
    unsafe { ptr::write(this, rust_slice) }
}

#[unsafe(export_name = "cxxbridge1$slice$ptr")]
unsafe extern "C" fn slice_ptr(this: &RustSlice) -> NonNull<()> {
    this.as_non_null_ptr()
}

#[unsafe(export_name = "cxxbridge1$slice$len")]
unsafe extern "C" fn slice_len(this: &RustSlice) -> usize {
    this.len()
}
