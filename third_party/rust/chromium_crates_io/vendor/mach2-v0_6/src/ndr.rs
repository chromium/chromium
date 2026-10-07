//! This module roughly corresponds to `mach/ndr.h`.

use core::ffi::c_uchar;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
#[allow(dead_code)]
pub struct NDR_record_t {
    mig_vers: c_uchar,
    if_vers: c_uchar,
    reserved1: c_uchar,
    mig_encoding: c_uchar,
    int_rep: c_uchar,
    char_rep: c_uchar,
    float_rep: c_uchar,
    reserved32: c_uchar,
}

unsafe extern "C" {
    pub static NDR_record: NDR_record_t;
}
