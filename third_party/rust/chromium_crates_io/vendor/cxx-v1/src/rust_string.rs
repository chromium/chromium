#![cfg(feature = "alloc")]
#![allow(missing_docs)]

use alloc::string::String;
use core::mem::{self, MaybeUninit};
use core::ptr;

// ABI compatible with C++ rust::String (not necessarily alloc::string::String).
#[repr(C)]
pub struct RustString {
    repr: [MaybeUninit<usize>; mem::size_of::<String>() / mem::size_of::<usize>()],
}

impl RustString {
    pub fn from(s: String) -> Self {
        unsafe { mem::transmute::<String, RustString>(s) }
    }

    pub fn from_ref(s: &String) -> &Self {
        unsafe { &*(ptr::from_ref::<String>(s).cast::<RustString>()) }
    }

    pub fn from_mut(s: &mut String) -> &mut Self {
        unsafe { &mut *(ptr::from_mut::<String>(s).cast::<RustString>()) }
    }

    pub fn into_string(self) -> String {
        unsafe { mem::transmute::<RustString, String>(self) }
    }

    pub fn as_string(&self) -> &String {
        unsafe { &*(ptr::from_ref::<RustString>(self).cast::<String>()) }
    }

    pub fn as_mut_string(&mut self) -> &mut String {
        unsafe { &mut *(ptr::from_mut::<RustString>(self).cast::<String>()) }
    }
}

impl Drop for RustString {
    fn drop(&mut self) {
        unsafe { ptr::drop_in_place(self.as_mut_string()) }
    }
}

const_assert_eq!(mem::size_of::<[usize; 3]>(), mem::size_of::<RustString>());
const_assert_eq!(mem::size_of::<String>(), mem::size_of::<RustString>());
const_assert_eq!(mem::align_of::<String>(), mem::align_of::<RustString>());
