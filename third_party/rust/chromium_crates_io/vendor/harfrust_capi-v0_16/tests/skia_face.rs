//! Regression tests for Skia's face setup and callback arrangement.

use crate::*;
use core::ffi::c_void;
use std::{
    ptr,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
};

unsafe fn face() -> *mut hr_face_t {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../harfrust/tests/fonts/rb_custom/LaBelleAurore.ttf");
    let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
    let blob = unsafe { hr_blob_create_from_file(path.as_ptr()) };
    let face = unsafe { hr_face_create(blob, 0) };
    unsafe { hr_blob_destroy(blob) };
    face
}

#[test]
fn index_assignment_changes_metadata_without_changing_tables() {
    unsafe {
        let face = face();
        let head = hr_face_reference_table(face, u32::from_be_bytes(*b"head"));
        assert!(hr_blob_get_length(head) > 0);
        hr_face_set_index(face, 1234);
        assert_eq!(hr_face_get_index(face), 1234);
        let same = hr_face_reference_table(face, u32::from_be_bytes(*b"head"));
        let mut len = 0;
        let before = hr_blob_get_data(head, &raw mut len);
        let after = hr_blob_get_data(same, ptr::null_mut());
        assert_eq!(
            std::slice::from_raw_parts(before, len as usize),
            std::slice::from_raw_parts(after, len as usize)
        );
        let font = hr_font_create(face);
        assert_eq!(
            hr_font_get_nominal_glyph(font, 'a' as u32, ptr::null_mut()),
            1
        );
        hr_face_make_immutable(face);
        hr_face_set_index(face, 0);
        assert_eq!(hr_face_get_index(face), 1234);
        hr_face_set_index(ptr::null_mut(), 2);
        hr_face_set_index(hr_face_get_empty(), 2);
        assert_eq!(hr_face_get_index(hr_face_get_empty()), 0);
        hr_font_destroy(font);
        hr_blob_destroy(same);
        hr_blob_destroy(head);
        hr_face_destroy(face);
    }
}

struct Tables {
    face: *mut hr_face_t,
    drops: Arc<AtomicUsize>,
}

unsafe extern "C" fn table(_face: *mut hr_face_t, tag: u32, data: *mut c_void) -> *mut hr_blob_t {
    unsafe { hr_face_reference_table((*data.cast::<Tables>()).face, tag) }
}

unsafe extern "C" fn tags(
    _face: *const hr_face_t,
    start: u32,
    count: *mut u32,
    tags: *mut u32,
    data: *mut c_void,
) -> u32 {
    unsafe { hr_face_get_table_tags((*data.cast::<Tables>()).face, start, count, tags) }
}

unsafe extern "C" fn drop_tables(data: *mut c_void) {
    let data = unsafe { Box::from_raw(data.cast::<Tables>()) };
    data.drops.fetch_add(1, Ordering::Relaxed);
    unsafe { hr_face_destroy(data.face) };
}

#[test]
fn table_enumeration_paginates_and_owns_callbacks() {
    unsafe {
        let original = face();
        let total = hr_face_get_table_tags(original, 0, ptr::null_mut(), ptr::null_mut());
        assert!(total > 2);
        let mut all = vec![0; total as usize];
        let mut count = total;
        assert_eq!(
            hr_face_get_table_tags(original, 0, &raw mut count, all.as_mut_ptr()),
            total
        );
        assert_eq!(count, total);
        hr_face_set_index(original, 1234);
        let mut last = [0; 2];
        count = 2;
        assert_eq!(
            hr_face_get_table_tags(original, total - 1, &raw mut count, last.as_mut_ptr()),
            total
        );
        assert_eq!((count, last[0]), (1, all[total as usize - 1]));
        count = 2;
        assert_eq!(
            hr_face_get_table_tags(original, total + 1, &raw mut count, last.as_mut_ptr()),
            total
        );
        assert_eq!(count, 0);

        let drops = Arc::new(AtomicUsize::new(0));
        let data = || {
            Box::into_raw(Box::new(Tables {
                face: hr_face_reference(original),
                drops: Arc::clone(&drops),
            }))
            .cast()
        };
        let callback_face = hr_face_create_for_tables(Some(table), data(), Some(drop_tables));
        assert_eq!(
            hr_face_get_table_tags(callback_face, 0, ptr::null_mut(), ptr::null_mut()),
            0
        );
        hr_face_set_get_table_tags_func(callback_face, Some(tags), data(), Some(drop_tables));
        assert_eq!(
            hr_face_get_table_tags(callback_face, 0, ptr::null_mut(), ptr::null_mut()),
            total
        );
        hr_face_set_get_table_tags_func(callback_face, Some(tags), data(), Some(drop_tables));
        assert_eq!(drops.load(Ordering::Relaxed), 1);
        hr_face_make_immutable(callback_face);
        hr_face_set_get_table_tags_func(callback_face, Some(tags), data(), Some(drop_tables));
        assert_eq!(drops.load(Ordering::Relaxed), 2);
        hr_face_destroy(callback_face);
        assert_eq!(drops.load(Ordering::Relaxed), 4);
        hr_face_destroy(original);
    }
}

#[test]
fn default_unicode_scripts_match_itemization_needs() {
    let provider = hr_unicode_funcs_get_default();
    assert!(!provider.is_null());
    assert_eq!(provider, hr_unicode_funcs_get_default());
    for (codepoint, script) in [
        ('a' as u32, HR_SCRIPT_LATIN),
        (0x0627, HR_SCRIPT_ARABIC),
        (0x4E00, HR_SCRIPT_HAN),
        (0x0301, HR_SCRIPT_INHERITED),
        (0x0020, HR_SCRIPT_COMMON),
        (0x0010_FFFF, HR_SCRIPT_UNKNOWN),
        (0x0011_0000, HR_SCRIPT_UNKNOWN),
        (u32::MAX, HR_SCRIPT_UNKNOWN),
    ] {
        assert_eq!(hr_unicode_script(provider, codepoint), script);
        assert_eq!(hr_unicode_script(ptr::null_mut(), codepoint), script);
    }
}

#[test]
fn unicode_collection_adds_coverage_without_clearing_the_set() {
    unsafe {
        let face = face();
        let set = hr_set_create();
        hr_set_add(set, 0x0010_FFFF);
        hr_face_collect_unicodes(face, set);
        for codepoint in ['A', 'a', 'b'] {
            assert_eq!(hr_set_has(set, codepoint as u32), 1);
        }
        assert_eq!(hr_set_has(set, 0x0010_FFFF), 1);
        let population = hr_set_get_population(set);
        assert!(population > 3);
        hr_face_collect_unicodes(face, set);
        hr_face_collect_unicodes(ptr::null_mut(), set);
        hr_face_collect_unicodes(face, ptr::null_mut());
        hr_face_collect_unicodes(face, hr_set_get_empty());
        assert_eq!(hr_set_get_population(set), population);
        hr_set_invert(set);
        hr_set_del(set, 'a' as u32);
        hr_face_collect_unicodes(face, set);
        assert_eq!(hr_set_has(set, 'a' as u32), 1);
        hr_set_destroy(set);
        hr_face_destroy(face);
    }
}

#[test]
fn unicode_collection_covers_constant_format13_ranges() {
    unsafe {
        let data = include_bytes!("../../harfrust/tests/fonts/in-house/AdobeBlank2.ttf");
        let blob = hr_blob_create(
            data.as_ptr().cast(),
            data.len() as u32,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        );
        let face = hr_face_create(blob, 0);
        hr_blob_destroy(blob);
        let set = hr_set_create();
        hr_face_collect_unicodes(face, set);
        assert_eq!(hr_set_get_population(set), 1_111_998);
        assert_eq!(hr_set_has(set, 0), 1);
        assert_eq!(hr_set_has(set, 0x0010_FFFD), 1);
        hr_set_destroy(set);
        hr_face_destroy(face);
    }
}
