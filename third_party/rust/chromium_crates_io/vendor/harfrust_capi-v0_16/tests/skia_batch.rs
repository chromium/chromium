//! Tests for batched nominal glyph mapping and advances.

use crate::*;
use core::ffi::{c_uint, c_void};
use std::ptr;

unsafe fn face() -> *mut hr_face_t {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../harfrust/tests/fonts/rb_custom/LaBelleAurore.ttf");
    let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
    let blob = unsafe { hr_blob_create_from_file(path.as_ptr()) };
    let face = unsafe { hr_face_create(blob, 0) };
    unsafe { hr_blob_destroy(blob) };
    face
}

unsafe extern "C" fn batch(
    font: *mut hr_font_t,
    data: *mut c_void,
    count: c_uint,
    unicodes: *const u32,
    unicode_stride: c_uint,
    glyphs: *mut u32,
    glyph_stride: c_uint,
    user: *mut c_void,
) -> c_uint {
    assert_eq!(data, user);
    assert!(!unsafe { hr_font_get_face(font) }.is_null());
    for i in 0..count as usize {
        let c = unsafe {
            unicodes
                .byte_add(i * unicode_stride as usize)
                .read_unaligned()
        };
        if c == 'z' as u32 {
            return i as c_uint;
        }
        unsafe {
            glyphs
                .byte_add(i * glyph_stride as usize)
                .write_unaligned(c + 100);
        };
    }
    count
}

unsafe extern "C" fn scalar(
    _font: *mut hr_font_t,
    _data: *mut c_void,
    c: u32,
    glyph: *mut u32,
    _user: *mut c_void,
) -> i32 {
    unsafe { glyph.write(c + 200) };
    1
}

#[test]
fn nominal_batches_preserve_strides_partial_results_and_parent_precedence() {
    unsafe {
        let face = face();
        let parent = hr_font_create(face);
        let funcs = hr_font_funcs_create();
        let mut data = 42u32;
        hr_font_funcs_set_nominal_glyphs_func(funcs, Some(batch), (&raw mut data).cast(), None);
        hr_font_set_funcs(parent, funcs, (&raw mut data).cast(), None);
        hr_font_funcs_destroy(funcs);
        let child = hr_font_create_sub_font(parent);
        let unicodes = ['a' as u32, 999, 'b' as u32, 999, 'z' as u32, 999];
        let mut glyphs = [777u32; 6];
        assert_eq!(
            hr_font_get_nominal_glyphs(child, 3, unicodes.as_ptr(), 8, glyphs.as_mut_ptr(), 8),
            2
        );
        assert_eq!(glyphs, [197, 777, 198, 777, 777, 777]);
        let mut glyph = 0;
        assert_eq!(
            hr_font_get_nominal_glyph(child, 'a' as u32, &raw mut glyph),
            1
        );
        assert_eq!(glyph, 197);
        assert_eq!(
            hr_font_get_nominal_glyphs(child, 0, ptr::null(), 0, ptr::null_mut(), 0),
            0
        );
        assert_eq!(
            hr_font_get_nominal_glyphs(child, 2, unicodes.as_ptr(), 0, &raw mut glyph, 0),
            2
        );
        assert_eq!(glyph, 197);

        let buffer = hr_buffer_create();
        hr_buffer_add_utf8(buffer, c"abc".as_ptr(), 3, 0, 3);
        hr_buffer_guess_segment_properties(buffer);
        hr_shape(child, buffer, ptr::null(), 0);
        let mut length = 0;
        let infos = hr_buffer_get_glyph_infos(buffer, &raw mut length);
        assert_eq!(length, 3);
        for i in 0..3 {
            assert_eq!((*infos.add(i)).codepoint, 197 + i as u32);
        }
        hr_buffer_destroy(buffer);

        let funcs = hr_font_funcs_create();
        hr_font_funcs_set_nominal_glyph_func(funcs, Some(scalar), ptr::null_mut(), None);
        hr_font_set_funcs(child, funcs, ptr::null_mut(), None);
        hr_font_funcs_destroy(funcs);
        assert_eq!(
            hr_font_get_nominal_glyphs(child, 1, unicodes.as_ptr(), 4, &raw mut glyph, 4),
            1
        );
        assert_eq!(glyph, 297); // Child scalar overrides parent batch.
        assert_eq!(
            hr_font_get_nominal_glyph(child, 'a' as u32, &raw mut glyph),
            1
        );
        assert_eq!(glyph, 297);
        hr_font_destroy(child);
        hr_font_destroy(parent);
        hr_face_destroy(face);
    }
}

unsafe extern "C" fn spaced_advance(
    font: *mut hr_font_t,
    _data: *mut c_void,
    glyph: u32,
    _user: *mut c_void,
) -> i32 {
    unsafe { hr_font_get_glyph_h_advance(hr_font_get_parent(font), glyph) + 7 }
}

// Written the way HarfBuzz's own default batch is: one scalar query a glyph.
unsafe extern "C" fn batch_advance_over_scalar(
    font: *mut hr_font_t,
    _data: *mut c_void,
    count: c_uint,
    glyphs: *const u32,
    glyph_stride: c_uint,
    advances: *mut i32,
    advance_stride: c_uint,
    _user: *mut c_void,
) {
    for i in 0..count as usize {
        let glyph = unsafe { glyphs.byte_add(i * glyph_stride as usize).read_unaligned() };
        let advance = unsafe { hr_font_get_glyph_h_advance(font, glyph) };
        unsafe {
            advances
                .byte_add(i * advance_stride as usize)
                .write_unaligned(advance);
        };
    }
}

unsafe extern "C" fn batch_advance_of_500(
    _font: *mut hr_font_t,
    _data: *mut c_void,
    count: c_uint,
    _glyphs: *const u32,
    _glyph_stride: c_uint,
    advances: *mut i32,
    advance_stride: c_uint,
    _user: *mut c_void,
) {
    for i in 0..count as usize {
        unsafe {
            advances
                .byte_add(i * advance_stride as usize)
                .write_unaligned(500);
        };
    }
}

#[test]
fn scalar_advance_callbacks_take_precedence_over_batch_ones() {
    unsafe {
        let face = face();
        let parent = hr_font_create(face);
        let funcs = hr_font_funcs_create();
        hr_font_funcs_set_glyph_h_advances_func(
            funcs,
            Some(batch_advance_of_500),
            ptr::null_mut(),
            None,
        );
        hr_font_set_funcs(parent, funcs, ptr::null_mut(), None);
        hr_font_funcs_destroy(funcs);
        let glyphs = [1u32, 2];
        let mut advances = [0i32; 2];

        // A child's scalar callback overrides the parent's batch.
        let child = hr_font_create_sub_font(parent);
        let funcs = hr_font_funcs_create();
        hr_font_funcs_set_glyph_h_advance_func(funcs, Some(spaced_advance), ptr::null_mut(), None);
        hr_font_set_funcs(child, funcs, ptr::null_mut(), None);
        hr_font_funcs_destroy(funcs);
        assert_eq!(hr_font_get_glyph_h_advance(child, 1), 507);
        hr_font_get_glyph_h_advances(child, 2, glyphs.as_ptr(), 4, advances.as_mut_ptr(), 4);
        assert_eq!(advances, [507, 507]);
        hr_font_destroy(child);

        // A scalar query goes to the scalar callback beside a batch one, so
        // the batch callback can be written over scalar queries.
        let child = hr_font_create_sub_font(parent);
        let funcs = hr_font_funcs_create();
        hr_font_funcs_set_glyph_h_advance_func(funcs, Some(spaced_advance), ptr::null_mut(), None);
        hr_font_funcs_set_glyph_h_advances_func(
            funcs,
            Some(batch_advance_over_scalar),
            ptr::null_mut(),
            None,
        );
        hr_font_set_funcs(child, funcs, ptr::null_mut(), None);
        hr_font_funcs_destroy(funcs);
        assert_eq!(hr_font_get_glyph_h_advance(child, 1), 507);
        hr_font_get_glyph_h_advances(child, 2, glyphs.as_ptr(), 4, advances.as_mut_ptr(), 4);
        assert_eq!(advances, [507, 507]);
        hr_font_destroy(child);

        hr_font_destroy(parent);
        hr_face_destroy(face);
    }
}
