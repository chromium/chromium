//! Exercise ownership, fallback, transforms and rendering through the C ABI.
use crate::*;
use core::ffi::c_void;
use std::ptr;

unsafe fn font() -> *mut hr_font_t {
    let bytes = include_bytes!("fonts/Rendering.ttf");
    unsafe {
        let blob = hr_blob_create(
            bytes.as_ptr().cast(),
            bytes.len() as u32,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        );
        let face = hr_face_create(blob, 0);
        let font = hr_font_create(face);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
        font
    }
}

#[derive(Default, Debug)]
struct Drawing(Vec<(u8, Vec<f32>, hr_draw_state_t)>);
unsafe extern "C" fn move_to(
    _: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    x: f32,
    y: f32,
    _: *mut c_void,
) {
    unsafe {
        (*data.cast::<Drawing>()).0.push((0, vec![x, y], *st));
    }
}
unsafe extern "C" fn line_to(
    _: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    x: f32,
    y: f32,
    _: *mut c_void,
) {
    unsafe {
        (*data.cast::<Drawing>()).0.push((1, vec![x, y], *st));
    }
}
unsafe extern "C" fn cubic_to(
    _: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    a: f32,
    b: f32,
    c: f32,
    d: f32,
    x: f32,
    y: f32,
    _: *mut c_void,
) {
    unsafe {
        (*data.cast::<Drawing>())
            .0
            .push((2, vec![a, b, c, d, x, y], *st));
    }
}
unsafe extern "C" fn close(
    _: *mut hr_draw_funcs_t,
    data: *mut c_void,
    st: *mut hr_draw_state_t,
    _: *mut c_void,
) {
    unsafe {
        (*data.cast::<Drawing>()).0.push((3, vec![], *st));
    }
}
unsafe fn draw_funcs() -> *mut hr_draw_funcs_t {
    unsafe {
        let funcs = hr_draw_funcs_create();
        hr_draw_funcs_set_move_to_func(funcs, Some(move_to), ptr::null_mut(), None);
        hr_draw_funcs_set_line_to_func(funcs, Some(line_to), ptr::null_mut(), None);
        hr_draw_funcs_set_cubic_to_func(funcs, Some(cubic_to), ptr::null_mut(), None);
        hr_draw_funcs_set_close_path_func(funcs, Some(close), ptr::null_mut(), None);
        funcs
    }
}

#[test]
#[allow(clippy::float_cmp)]
fn draw_state_quadratic_fallback_and_contour_closure() {
    unsafe {
        let funcs = draw_funcs();
        let mut drawing = Drawing::default();
        let data = ptr::from_mut(&mut drawing).cast();
        let mut state = hr_draw_state_t::default();
        hr_draw_move_to(funcs, data, &raw mut state, 3.0, 6.0);
        assert!(drawing.0.is_empty());
        hr_draw_quadratic_to(funcs, data, &raw mut state, 9.0, 12.0, 15.0, 18.0);
        assert_eq!(drawing.0[0].2.path_open, 0);
        assert_eq!(drawing.0[1].1, [7.0, 10.0, 11.0, 14.0, 15.0, 18.0]);
        assert_eq!(
            (drawing.0[1].2.current_x, drawing.0[1].2.current_y),
            (3.0, 6.0)
        );
        hr_draw_close_path(funcs, data, &raw mut state);
        assert_eq!(drawing.0[2].1, [3.0, 6.0]);
        assert_eq!(
            (drawing.0[3].2.current_x, drawing.0[3].2.current_y),
            (3.0, 6.0)
        );
        assert_eq!(state.path_open, 0);
        assert_eq!(state.current_x, 0.0);
        hr_draw_close_path(funcs, data, &raw mut state);
        assert_eq!(drawing.0.len(), 4);
        hr_draw_funcs_destroy(funcs);
    }
}

unsafe extern "C" fn table(_: *mut hr_face_t, tag: u32, data: *mut c_void) -> *mut hr_blob_t {
    unsafe { hr_face_reference_table(data.cast(), tag) }
}

#[test]
#[allow(clippy::float_cmp)]
fn outlines_support_callback_faces_without_table_enumeration() {
    unsafe {
        let original = font();
        let callback =
            hr_face_create_for_tables(Some(table), hr_font_get_face(original).cast(), None);
        hr_face_set_index(callback, 42);
        let font = hr_font_create(callback);
        hr_face_destroy(callback);
        hr_font_set_scale(font, 2000, -500);
        let funcs = draw_funcs();
        let mut drawing = Drawing::default();
        assert_eq!(
            hr_font_draw_glyph_or_fail(font, 1, funcs, ptr::from_mut(&mut drawing).cast()),
            1
        );
        assert_eq!(
            drawing.0.iter().map(|e| e.0).collect::<Vec<_>>(),
            [0, 1, 1, 1, 3]
        );
        assert_eq!(drawing.0[0].1, [200.0, 0.0]);
        assert_eq!(drawing.0[1].1, [1000.0, -400.0]);
        assert_eq!(drawing.0[2].1, [1800.0, 0.0]);
        drawing.0.clear();
        assert_eq!(
            hr_font_draw_glyph_or_fail(font, u32::MAX, funcs, ptr::from_mut(&mut drawing).cast()),
            0
        );
        assert!(drawing.0.is_empty());
        hr_draw_funcs_destroy(funcs);
        hr_font_destroy(font);
        hr_font_destroy(original);
    }
}

/// Overlapping table records would otherwise have the whole font copied into
/// the rendering cache once per tag, so such a face is not rendered.
#[test]
fn overlapping_tables_are_not_copied_once_per_tag() {
    // Every record of the directory runs to the end of the file. The tables
    // still start at their real data, so the font parses and shapes.
    let mut bytes = include_bytes!("fonts/Rendering.ttf").to_vec();
    let len = u32::try_from(bytes.len()).unwrap();
    let count = usize::from(u16::from_be_bytes([bytes[4], bytes[5]]));
    for i in 0..count {
        let record = 12 + 16 * i;
        let offset = u32::from_be_bytes(bytes[record + 8..record + 12].try_into().unwrap());
        bytes[record + 12..record + 16].copy_from_slice(&(len - offset).to_be_bytes());
    }
    unsafe {
        let blob = hr_blob_create(
            bytes.as_ptr().cast(),
            len,
            HR_MEMORY_MODE_READONLY,
            ptr::null_mut(),
            None,
        );
        let face = hr_face_create(blob, 0);
        assert_ne!(hr_face_get_glyph_count(face), 0);
        let font = hr_font_create(face);
        let funcs = draw_funcs();
        let mut drawing = Drawing::default();
        assert_eq!(
            hr_font_draw_glyph_or_fail(font, 1, funcs, ptr::from_mut(&mut drawing).cast()),
            0
        );
        assert!(drawing.0.is_empty());
        hr_draw_funcs_destroy(funcs);
        hr_font_destroy(font);
        hr_face_destroy(face);
        hr_blob_destroy(blob);
    }
}

#[test]
fn outlines_are_built_once_per_face() {
    unsafe {
        let font = font();
        let face = &*hr_font_get_face(font);
        assert!(face.render_data.get().is_none());
        let funcs = draw_funcs();
        let mut drawing = Drawing::default();
        let data = ptr::from_mut(&mut drawing).cast();
        assert_eq!(hr_font_draw_glyph_or_fail(font, 1, funcs, data), 1);
        let first = rendering::outlines(face).map(ptr::from_ref);
        assert!(first.is_some());
        let segments = drawing.0.len();
        assert_eq!(hr_font_draw_glyph_or_fail(font, 1, funcs, data), 1);
        assert_eq!(drawing.0.len(), 2 * segments);
        // The second draw reused the collection instead of parsing the font
        // tables again; for a CFF font that walk covers the whole Top DICT.
        assert_eq!(rendering::outlines(face).map(ptr::from_ref), first);
        hr_draw_funcs_destroy(funcs);
        hr_font_destroy(font);
    }
}

unsafe extern "C" fn custom_draw(
    _: *mut hr_font_t,
    _: *mut c_void,
    _: u32,
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    _: *mut c_void,
) -> i32 {
    let mut state = hr_draw_state_t::default();
    unsafe {
        hr_draw_move_to(funcs, data, &raw mut state, 10.0, 20.0);
        hr_draw_line_to(funcs, data, &raw mut state, 30.0, 40.0);
        hr_draw_close_path(funcs, data, &raw mut state);
    }
    1
}

unsafe extern "C" fn count_drop(data: *mut c_void) {
    unsafe {
        *data.cast::<u32>() += 1;
    }
}

#[test]
#[allow(clippy::float_cmp)]
fn custom_draw_inherits_parent_scale_and_owns_callback_data() {
    unsafe {
        let font = font();
        let callbacks = hr_font_funcs_create();
        let mut drops = 0;
        hr_font_funcs_set_draw_glyph_or_fail_func(
            callbacks,
            Some(custom_draw),
            ptr::from_mut(&mut drops).cast(),
            Some(count_drop),
        );
        hr_font_set_funcs(font, callbacks, ptr::null_mut(), None);
        hr_font_funcs_destroy(callbacks);
        let child = hr_font_create_sub_font(font);
        hr_font_set_scale(child, 2000, -500);
        let funcs = draw_funcs();
        let mut drawing = Drawing::default();
        assert_eq!(
            hr_font_draw_glyph_or_fail(child, 1, funcs, ptr::from_mut(&mut drawing).cast()),
            1
        );
        assert_eq!(drawing.0[0].1, [20.0, -10.0]);
        assert_eq!(drawing.0[1].1, [60.0, -20.0]);
        hr_ot_font_set_funcs(child);
        drawing.0.clear();
        assert_eq!(
            hr_font_draw_glyph_or_fail(child, 1, funcs, ptr::from_mut(&mut drawing).cast()),
            1
        );
        assert_eq!(drawing.0[0].1, [200.0, 0.0]);
        hr_font_destroy(child);
        hr_font_destroy(font);
        assert_eq!(drops, 1);
        hr_draw_funcs_make_immutable(funcs);
        hr_draw_funcs_set_move_to_func(
            funcs,
            Some(move_to),
            ptr::from_mut(&mut drops).cast(),
            Some(count_drop),
        );
        assert_eq!(drops, 2);
        hr_draw_funcs_set_move_to_func(
            ptr::null_mut(),
            None,
            ptr::from_mut(&mut drops).cast(),
            Some(count_drop),
        );
        assert_eq!(drops, 3);
        hr_draw_funcs_destroy(funcs);
    }
}

// Lines the caller has received, and how many had arrived while the parent's
// callback was still drawing.
#[derive(Default)]
struct Streamed {
    delivered: usize,
    during_draw: usize,
}

unsafe extern "C" fn count_line(
    _: *mut hr_draw_funcs_t,
    data: *mut c_void,
    _: *mut hr_draw_state_t,
    _: f32,
    _: f32,
    _: *mut c_void,
) {
    unsafe { (*data.cast::<Streamed>()).delivered += 1 };
}

unsafe extern "C" fn custom_draw_observing_delivery(
    _: *mut hr_font_t,
    _: *mut c_void,
    _: u32,
    funcs: *mut hr_draw_funcs_t,
    data: *mut c_void,
    streamed: *mut c_void,
) -> i32 {
    let mut state = hr_draw_state_t::default();
    unsafe {
        hr_draw_move_to(funcs, data, &raw mut state, 10.0, 20.0);
        hr_draw_line_to(funcs, data, &raw mut state, 30.0, 40.0);
        let streamed = streamed.cast::<Streamed>();
        (*streamed).during_draw = (*streamed).delivered;
        hr_draw_close_path(funcs, data, &raw mut state);
    }
    1
}

// An outline can be far longer than its font, so a sub-font must pass segments
// on as its parent draws them rather than hold the outline until it returns.
#[test]
fn sub_font_draw_streams_segments() {
    unsafe {
        let font = font();
        let callbacks = hr_font_funcs_create();
        let mut streamed = Streamed::default();
        hr_font_funcs_set_draw_glyph_or_fail_func(
            callbacks,
            Some(custom_draw_observing_delivery),
            ptr::from_mut(&mut streamed).cast(),
            None,
        );
        hr_font_set_funcs(font, callbacks, ptr::null_mut(), None);
        hr_font_funcs_destroy(callbacks);
        let child = hr_font_create_sub_font(font);
        let funcs = hr_draw_funcs_create();
        hr_draw_funcs_set_line_to_func(funcs, Some(count_line), ptr::null_mut(), None);
        assert_eq!(
            hr_font_draw_glyph_or_fail(child, 1, funcs, ptr::from_mut(&mut streamed).cast()),
            1
        );
        assert_eq!(streamed.during_draw, 1);
        hr_draw_funcs_destroy(funcs);
        hr_font_destroy(child);
        hr_font_destroy(font);
    }
}

#[cfg(feature = "paint")]
mod painting {
    use super::*;

    #[derive(Default)]
    struct Paint {
        scopes: Vec<u8>,
        transforms: Vec<(f32, f32)>,
        colors: Vec<(i32, u32)>,
        stops: Vec<hr_color_stop_t>,
        gradients: Vec<u8>,
        image: *mut hr_blob_t,
        image_format: u32,
        extents: Option<hr_glyph_extents_t>,
        custom: bool,
        strict: bool,
        color_glyphs: u32,
    }
    unsafe fn state<'a>(data: *mut c_void) -> &'a mut Paint {
        unsafe { &mut *data.cast() }
    }
    unsafe extern "C" fn push_transform(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        xx: f32,
        _: f32,
        _: f32,
        yy: f32,
        _: f32,
        _: f32,
        _: *mut c_void,
    ) {
        unsafe {
            state(data).transforms.push((xx, yy));
            state(data).scopes.push(0);
        }
    }
    unsafe extern "C" fn pop_transform(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        _: *mut c_void,
    ) {
        unsafe {
            assert_eq!(state(data).scopes.pop(), Some(0));
        }
    }
    unsafe extern "C" fn push_clip(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        _: u32,
        _: *mut hr_font_t,
        _: *mut c_void,
    ) {
        unsafe {
            state(data).scopes.push(1);
        }
    }
    unsafe extern "C" fn pop_clip(_: *mut hr_paint_funcs_t, data: *mut c_void, _: *mut c_void) {
        unsafe {
            assert_eq!(state(data).scopes.pop(), Some(1));
        }
    }
    unsafe extern "C" fn color(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        foreground: i32,
        color: u32,
        _: *mut c_void,
    ) {
        unsafe {
            state(data).colors.push((foreground, color));
        }
    }
    unsafe extern "C" fn custom(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        index: u32,
        color: *mut u32,
        _: *mut c_void,
    ) -> i32 {
        unsafe {
            if state(data).custom && index == 0 {
                *color = 0x00ff_00ff;
                1
            } else {
                0
            }
        }
    }
    unsafe fn stops(data: *mut c_void, line: *mut hr_color_line_t, kind: u8) {
        unsafe {
            let total = hr_color_line_get_color_stops(line, 0, ptr::null_mut(), ptr::null_mut());
            if state(data).strict {
                assert_eq!(total, 2);
            }
            let mut count = total;
            let mut stops = vec![hr_color_stop_t::default(); total as usize];
            assert_eq!(
                hr_color_line_get_color_stops(line, 0, &raw mut count, stops.as_mut_ptr()),
                total
            );
            assert_eq!(count, total);
            count = 99;
            hr_color_line_get_color_stops(line, total + 1, &raw mut count, ptr::null_mut());
            assert_eq!(count, 0);
            let extend = hr_color_line_get_extend(line);
            if state(data).strict {
                assert_eq!(extend, HR_PAINT_EXTEND_PAD);
            } else {
                assert!(extend <= HR_PAINT_EXTEND_REFLECT);
            }
            state(data).stops.extend_from_slice(&stops);
            state(data).gradients.push(kind);
        }
    }
    unsafe extern "C" fn linear(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        line: *mut hr_color_line_t,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: *mut c_void,
    ) {
        unsafe {
            stops(data, line, 0);
        }
    }
    unsafe extern "C" fn radial(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        line: *mut hr_color_line_t,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: *mut c_void,
    ) {
        unsafe {
            stops(data, line, 1);
        }
    }
    unsafe extern "C" fn sweep(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        line: *mut hr_color_line_t,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: *mut c_void,
    ) {
        unsafe {
            stops(data, line, 2);
        }
    }
    unsafe extern "C" fn image(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        image: *mut hr_blob_t,
        _: u32,
        _: u32,
        format: u32,
        _: f32,
        extents: *mut hr_glyph_extents_t,
        _: *mut c_void,
    ) -> i32 {
        unsafe {
            let s = state(data);
            s.image = hr_blob_reference(image);
            s.image_format = format;
            s.extents = extents.as_ref().copied();
        }
        1
    }
    unsafe extern "C" fn rectangle(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        _: f32,
        _: f32,
        _: f32,
        _: f32,
        _: *mut c_void,
    ) {
        unsafe {
            state(data).scopes.push(1);
        }
    }
    unsafe extern "C" fn group(_: *mut hr_paint_funcs_t, data: *mut c_void, _: *mut c_void) {
        unsafe {
            state(data).scopes.push(2);
        }
    }
    unsafe extern "C" fn pop_group(
        _: *mut hr_paint_funcs_t,
        data: *mut c_void,
        _: u32,
        _: *mut c_void,
    ) {
        unsafe {
            assert_eq!(state(data).scopes.pop(), Some(2));
        }
    }
    unsafe fn funcs() -> *mut hr_paint_funcs_t {
        unsafe {
            let f = hr_paint_funcs_create();
            hr_paint_funcs_set_push_transform_func(f, Some(push_transform), ptr::null_mut(), None);
            hr_paint_funcs_set_pop_transform_func(f, Some(pop_transform), ptr::null_mut(), None);
            hr_paint_funcs_set_push_clip_glyph_func(f, Some(push_clip), ptr::null_mut(), None);
            hr_paint_funcs_set_pop_clip_func(f, Some(pop_clip), ptr::null_mut(), None);
            hr_paint_funcs_set_push_clip_rectangle_func(f, Some(rectangle), ptr::null_mut(), None);
            hr_paint_funcs_set_push_group_func(f, Some(group), ptr::null_mut(), None);
            hr_paint_funcs_set_pop_group_func(f, Some(pop_group), ptr::null_mut(), None);
            hr_paint_funcs_set_color_func(f, Some(color), ptr::null_mut(), None);
            hr_paint_funcs_set_custom_palette_color_func(f, Some(custom), ptr::null_mut(), None);
            hr_paint_funcs_set_linear_gradient_func(f, Some(linear), ptr::null_mut(), None);
            hr_paint_funcs_set_radial_gradient_func(f, Some(radial), ptr::null_mut(), None);
            hr_paint_funcs_set_sweep_gradient_func(f, Some(sweep), ptr::null_mut(), None);
            hr_paint_funcs_set_image_func(f, Some(image), ptr::null_mut(), None);
            f
        }
    }

    #[test]
    fn palettes_gradients_and_error_cleanup() {
        unsafe {
            let font = font();
            let funcs = funcs();
            let mut s = Paint {
                strict: true,
                ..Paint::default()
            };
            let data = ptr::from_mut(&mut s).cast();
            assert_eq!(
                hr_font_paint_glyph_or_fail(font, 2, funcs, data, 0, 0xaabb_ccff),
                1
            );
            assert_eq!(s.colors, [(0, 0x0000_ffff), (1, 0xaabb_ccff)]);
            s.colors.clear();
            assert_eq!(
                hr_font_paint_glyph_or_fail(font, 2, funcs, data, 1, 0xaabb_ccff),
                1
            );
            assert_eq!(s.colors[0], (0, 0xff00_0080));
            s.custom = true;
            s.colors.clear();
            assert_eq!(
                hr_font_paint_glyph_or_fail(font, 2, funcs, data, 99, 0xaabb_ccff),
                1
            );
            assert_eq!(s.colors[0], (0, 0x00ff_00ff));
            hr_font_set_scale(font, 2000, -500);
            for glyph in [3, 7, 8] {
                assert_eq!(
                    hr_font_paint_glyph_or_fail(font, glyph, funcs, data, 0, 0xaabb_ccff),
                    1
                );
                assert_eq!(s.scopes, [] as [u8; 0]);
            }
            assert_eq!(s.gradients, [0, 1, 2]);
            for stops in s.stops.chunks_exact(2) {
                assert_eq!(stops.iter().filter(|s| s.is_foreground != 0).count(), 1);
                for stop in stops {
                    assert_eq!(
                        stop.color,
                        if stop.is_foreground != 0 {
                            0xaabb_cc80
                        } else {
                            0x00ff_00ff
                        }
                    );
                }
            }
            assert_eq!(hr_font_paint_glyph_or_fail(font, 4, funcs, data, 0, 0), 0);
            assert_eq!(s.scopes, [] as [u8; 0]);
            assert_eq!(
                hr_font_paint_glyph_or_fail(font, u32::MAX, funcs, data, 0, 0),
                0
            );
            assert_eq!(s.scopes, [] as [u8; 0]);
            hr_paint_funcs_destroy(funcs);
            hr_font_destroy(font);
        }
    }

    unsafe extern "C" fn custom_font_paint(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        _: u32,
        funcs: *mut hr_paint_funcs_t,
        data: *mut c_void,
        palette: u32,
        foreground: u32,
        _: *mut c_void,
    ) -> i32 {
        assert_eq!(font_data, font.cast());
        assert_eq!(palette, 2);
        unsafe {
            hr_paint_color(funcs, data, 1, foreground);
        }
        0
    }
    unsafe extern "C" fn legacy_font_paint(
        font: *mut hr_font_t,
        font_data: *mut c_void,
        glyph: u32,
        funcs: *mut hr_paint_funcs_t,
        data: *mut c_void,
        palette: u32,
        foreground: u32,
        user: *mut c_void,
    ) {
        unsafe {
            custom_font_paint(
                font, font_data, glyph, funcs, data, palette, foreground, user,
            );
        }
    }

    #[test]
    #[allow(clippy::float_cmp)]
    fn custom_font_paint_inheritance_legacy_callbacks_and_ownership() {
        unsafe {
            let font = font();
            let callbacks = hr_font_funcs_create();
            let mut drops = 0u32;
            let user = ptr::from_mut(&mut drops).cast();
            hr_font_funcs_set_paint_glyph_or_fail_func(
                callbacks,
                Some(custom_font_paint),
                user,
                Some(count_drop),
            );
            hr_font_set_funcs(font, callbacks, font.cast(), None);
            let child = hr_font_create_sub_font(font);
            hr_font_set_scale(child, 2000, -500);
            let funcs = funcs();
            let mut s = Paint::default();
            let data = ptr::from_mut(&mut s).cast();
            assert_eq!(
                hr_font_paint_glyph_or_fail(child, 1, funcs, data, 2, 0x1234_5678),
                0
            );
            assert_eq!(s.colors, [(1, 0x1234_5678)]);
            assert_eq!(s.transforms, [(2.0, -0.5)]);
            assert_eq!(s.scopes, [] as [u8; 0]);
            hr_font_funcs_set_paint_glyph_func(
                callbacks,
                Some(legacy_font_paint),
                user,
                Some(count_drop),
            );
            assert_eq!(drops, 1);
            assert_eq!(hr_font_paint_glyph_or_fail(child, 1, funcs, data, 2, 0), 1);
            hr_font_funcs_make_immutable(callbacks);
            hr_font_funcs_set_paint_glyph_or_fail_func(callbacks, None, user, Some(count_drop));
            assert_eq!(drops, 2);
            hr_font_funcs_destroy(callbacks);
            hr_font_destroy(child);
            hr_font_destroy(font);
            assert_eq!(drops, 3);
            hr_paint_funcs_set_color_func(funcs, Some(color), user, Some(count_drop));
            let retained = hr_paint_funcs_reference(funcs);
            hr_paint_funcs_destroy(funcs);
            assert_eq!(drops, 3);
            hr_paint_funcs_make_immutable(retained);
            hr_paint_funcs_set_color_func(retained, None, user, Some(count_drop));
            assert_eq!(drops, 4);
            hr_paint_funcs_destroy(retained);
            assert_eq!(drops, 5);
        }
    }

    #[test]
    fn colrv1_corpus_balances_nested_transforms_clips_and_composites() {
        unsafe {
            let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../harfrust/tests/fonts/rb_custom/test_glyphs-glyf_colr_1_no_cliplist.ttf");
            let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
            let blob = hr_blob_create_from_file(path.as_ptr());
            let face = hr_face_create(blob, 0);
            let font = hr_font_create(face);
            let funcs = funcs();
            let mut painted = 0;
            for glyph in 0..hr_face_get_glyph_count(face) {
                let mut s = Paint::default();
                painted += hr_font_paint_glyph_or_fail(
                    font,
                    glyph,
                    funcs,
                    ptr::from_mut(&mut s).cast(),
                    0,
                    0xff,
                );
                assert!(s.scopes.is_empty(), "glyph {glyph}");
            }
            // 200 COLRv1 entries include two intentional cycles; one COLRv0 glyph succeeds.
            assert_eq!(painted, 199);
            hr_paint_funcs_destroy(funcs);
            hr_font_destroy(font);
            hr_face_destroy(face);
            hr_blob_destroy(blob);
        }
    }

    unsafe extern "C" fn paint_color_glyph(
        funcs: *mut hr_paint_funcs_t,
        data: *mut c_void,
        glyph: u32,
        font: *mut hr_font_t,
        _: *mut c_void,
    ) -> hr_bool_t {
        unsafe {
            state(data).color_glyphs += 1;
            hr_font_paint_glyph_or_fail(font, glyph, funcs, data, 0, 0xff)
        }
    }

    #[test]
    fn color_glyph_callback_painting_sub_glyphs_stays_bounded() {
        unsafe {
            let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../harfrust/tests/fonts/rb_custom/test_glyphs-glyf_colr_1_no_cliplist.ttf");
            let path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
            let blob = hr_blob_create_from_file(path.as_ptr());
            let face = hr_face_create(blob, 0);
            let font = hr_font_create(face);
            let funcs = funcs();
            // The callback paints each sub-glyph through a nested traversal, the
            // way hb-cairo does. Glyphs 178 and 179 refer to each other, which
            // used to recurse until the stack overflowed.
            hr_paint_funcs_set_color_glyph_func(
                funcs,
                Some(paint_color_glyph),
                ptr::null_mut(),
                None,
            );
            let mut painted = 0;
            let mut color_glyphs = 0;
            for glyph in 0..hr_face_get_glyph_count(face) {
                let mut s = Paint::default();
                painted += hr_font_paint_glyph_or_fail(
                    font,
                    glyph,
                    funcs,
                    ptr::from_mut(&mut s).cast(),
                    0,
                    0xff,
                );
                assert!(s.scopes.is_empty(), "glyph {glyph}");
                // At most one callback per nested traversal the budget allows.
                assert!(s.color_glyphs <= 256, "glyph {glyph}");
                color_glyphs += s.color_glyphs;
            }
            assert_eq!(painted, 199);
            assert!(color_glyphs > 0);
            hr_paint_funcs_destroy(funcs);
            hr_font_destroy(font);
            hr_face_destroy(face);
            hr_blob_destroy(blob);
        }
    }

    #[test]
    fn retained_svg_and_bitmap_images_survive_font_destruction() {
        unsafe {
            for (glyph, format) in [(5, *b"svg "), (6, *b"png "), (9, *b"png ")] {
                let font = font();
                let funcs = funcs();
                let mut s = Paint::default();
                assert_eq!(
                    hr_font_paint_glyph_or_fail(
                        font,
                        glyph,
                        hr_paint_funcs_get_empty(),
                        ptr::null_mut(),
                        0,
                        0
                    ),
                    0
                );
                assert_eq!(
                    hr_font_paint_glyph_or_fail(
                        font,
                        glyph,
                        funcs,
                        ptr::from_mut(&mut s).cast(),
                        0,
                        0
                    ),
                    1
                );
                assert_eq!(s.image_format, u32::from_be_bytes(format));
                if glyph == 6 || glyph == 9 {
                    let e = s.extents.unwrap();
                    // At 1000 upem / 16 ppem, a pixel is 62.5 font units.
                    // Negative bearings and heights must round towards +inf.
                    assert_eq!(
                        (e.x_bearing, e.y_bearing, e.width, e.height),
                        if glyph == 6 {
                            (63, 188, 63, -62)
                        } else {
                            (-62, -62, 63, -62)
                        }
                    );
                } else {
                    assert!(s.extents.is_none());
                }
                hr_font_destroy(font);
                hr_paint_funcs_destroy(funcs);
                let mut len = 0;
                let bytes = hr_blob_get_data(s.image, &raw mut len);
                assert!(len > 50);
                let bytes = std::slice::from_raw_parts(bytes.cast::<u8>(), len as usize);
                assert!(bytes.starts_with(if glyph == 5 { b"<svg" } else { b"\x89PNG" }));
                hr_blob_destroy(s.image);
            }
        }
    }

    /// One SVG document may cover every glyph of a font, so images handed to
    /// the callback share the face's data rather than copying it per paint.
    #[test]
    fn retained_images_share_storage_rather_than_copying() {
        unsafe {
            for glyph in [5, 6] {
                let font = font();
                let funcs = funcs();
                let mut images = Vec::new();
                for _ in 0..2 {
                    let mut s = Paint::default();
                    assert_eq!(
                        hr_font_paint_glyph_or_fail(
                            font,
                            glyph,
                            funcs,
                            ptr::from_mut(&mut s).cast(),
                            0,
                            0
                        ),
                        1
                    );
                    images.push(s.image);
                }
                hr_font_destroy(font);
                hr_paint_funcs_destroy(funcs);
                let (mut a, mut b) = (0, 0);
                let pa = hr_blob_get_data(images[0], &raw mut a);
                let pb = hr_blob_get_data(images[1], &raw mut b);
                assert!(a > 50);
                assert_eq!((pa, a), (pb, b));
                for image in images {
                    hr_blob_destroy(image);
                }
            }
        }
    }
}
