// Other Unix (e.g. Linux) platforms use ELF as an object file format
// and typically implement an API called `dl_iterate_phdr` to load
// native libraries.

use super::mystd::borrow::ToOwned;
use super::mystd::env;
use super::mystd::ffi::{CStr, OsStr};
use super::mystd::os::unix::prelude::*;
use super::{Library, LibrarySegment, OsString, Vec};
use core::slice;

pub(super) fn native_libraries() -> Vec<Library> {
    let mut ret = Vec::new();
    unsafe {
        libc::dl_iterate_phdr(Some(callback), &mut ret as *mut Vec<_> as *mut _);
    }
    return ret;
}

// `info` should be a valid pointers.
// `vec` should be a valid pointer to a `std::Vec`.
unsafe extern "C" fn callback(
    info: *mut libc::dl_phdr_info,
    _size: libc::size_t,
    vec: *mut libc::c_void,
) -> libc::c_int {
    let info = &*info;
    let libs = &mut *(vec as *mut Vec<Library>);
    let is_main_prog = info.dlpi_name.is_null() || *info.dlpi_name == 0;
    let name = if is_main_prog {
        if libs.is_empty() {
            env::current_exe().map(|e| e.into()).unwrap_or_default()
        } else {
            OsString::new()
        }
    } else {
        let bytes = CStr::from_ptr(info.dlpi_name).to_bytes();
        OsStr::from_bytes(bytes).to_owned()
    };
    let headers = slice::from_raw_parts(info.dlpi_phdr, info.dlpi_phnum as usize);
    libs.push(Library {
        name,
        segments: headers
            .iter()
            .map(|header| LibrarySegment {
                len: (*header).p_memsz as usize,
                stated_virtual_memory_address: (*header).p_vaddr as usize,
            })
            .collect(),
        bias: info.dlpi_addr as usize,
    });
    0
}
