//! Synthetic table faces for metadata-query regression tests.

use crate::*;
use core::ffi::c_void;
use std::ptr;

pub(crate) struct TableFace(pub(crate) *mut hr_face_t);

impl TableFace {
    pub(crate) fn new(tables: Vec<(hr_tag_t, Vec<u8>)>) -> Self {
        unsafe extern "C" fn table(
            _face: *mut hr_face_t,
            tag: hr_tag_t,
            data: *mut c_void,
        ) -> *mut hr_blob_t {
            let tables = unsafe { &*data.cast::<Vec<(hr_tag_t, Vec<u8>)>>() };
            let Some((_, bytes)) = tables.iter().find(|(table, _)| *table == tag) else {
                return ptr::null_mut();
            };
            unsafe {
                hr_blob_create(
                    bytes.as_ptr().cast(),
                    bytes.len() as u32,
                    HR_MEMORY_MODE_DUPLICATE,
                    ptr::null_mut(),
                    None,
                )
            }
        }
        unsafe extern "C" fn destroy(data: *mut c_void) {
            drop(unsafe { Box::from_raw(data.cast::<Vec<(hr_tag_t, Vec<u8>)>>()) });
        }
        Self(unsafe {
            hr_face_create_for_tables(
                Some(table),
                Box::into_raw(Box::new(tables)).cast(),
                Some(destroy),
            )
        })
    }
}

impl Drop for TableFace {
    fn drop(&mut self) {
        unsafe { hr_face_destroy(self.0) };
    }
}
