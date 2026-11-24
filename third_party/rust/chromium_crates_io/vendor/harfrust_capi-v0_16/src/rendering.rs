//! Shared Skrifa input: an immutable SFNT assembled once per face.
//! Read known rendering tables directly, so callback faces need no enumeration.
// The C drawing/painting ABI uses f32 coordinates, including integer font scales.
#![allow(clippy::cast_precision_loss)]
use crate::*;
use core::ffi::c_void;
use harfrust::font::Blob;
use read_fonts::{types::F2Dot14, FontRef};

/// Skrifa's view of a face: the SFNT assembled from its tables, and the
/// outline collection that borrows it.
///
/// Building the collection parses the whole CFF Top DICT, whose length the
/// font chooses, so it is built once per face here rather than once per
/// glyph. Like `PreparedFont`, this keeps the view and the data it borrows in
/// one private owner; the explicit `Drop` clears the borrow before the blob is
/// released even if the fields are ever reordered.
pub(crate) struct RenderData {
    outlines: Option<skrifa::OutlineGlyphCollection<'static>>,
    blob: Blob,
}

impl RenderData {
    fn new(bytes: Vec<u8>) -> Option<Self> {
        let blob = Blob::from(bytes);
        let font = FontRef::new(&blob).ok()?;
        let outlines = skrifa::OutlineGlyphCollection::new(&font);
        // SAFETY: the collection borrows the data owned by `blob`, which is
        // reference counted, stable across moving the `Blob` and never written
        // again. `Drop` clears the borrow before the blob is released.
        let outlines = Some(unsafe {
            core::mem::transmute::<
                skrifa::OutlineGlyphCollection<'_>,
                skrifa::OutlineGlyphCollection<'static>,
            >(outlines)
        });
        Some(Self { outlines, blob })
    }
}

impl Drop for RenderData {
    fn drop(&mut self) {
        self.outlines = None;
    }
}

unsafe fn render_data(face: &hr_face_t) -> Option<&RenderData> {
    face.render_data
        .get_or_init(|| {
            unsafe { collect_tables(core::ptr::from_ref(face).cast_mut()) }
                .and_then(RenderData::new)
        })
        .as_ref()
}
/// The face's assembled SFNT, built on first use and never changed after.
pub(crate) unsafe fn render_blob(face: &hr_face_t) -> Option<&Blob> {
    unsafe { render_data(face) }.map(|data| &data.blob)
}
pub(crate) unsafe fn font_ref(face: &hr_face_t) -> Option<FontRef<'_>> {
    FontRef::new(unsafe { render_blob(face) }?).ok()
}
/// The face's outlines, built on first use and kept with the face.
pub(crate) unsafe fn outlines(face: &hr_face_t) -> Option<&skrifa::OutlineGlyphCollection<'_>> {
    unsafe { render_data(face) }?.outlines.as_ref()
}
unsafe fn collect_tables(face: *mut hr_face_t) -> Option<Vec<u8>> {
    const TAGS: [&[u8; 4]; 24] = [
        b"head", b"maxp", b"hhea", b"hmtx", b"vhea", b"vmtx", b"VORG", b"glyf", b"loca", b"gvar",
        b"HVAR", b"VVAR", b"CFF ", b"CFF2", b"VARC", b"fvar", b"avar", b"COLR", b"CPAL", b"CBLC",
        b"CBDT", b"sbix", b"SVG ", b"cmap",
    ];
    // A directory whose records overlap can list the same bytes under every
    // tag. Charge each table to the face's blob so that copying them cannot
    // cost more than one copy of the font; a callback face supplies its own
    // tables and has no such bound.
    let mut budget = unsafe { object::or_empty(face.cast_const()) }
        .blob_len()
        .unwrap_or(usize::MAX);
    let mut tables = Vec::new();
    for bytes in TAGS {
        let tag = u32::from_be_bytes(*bytes);
        let blob = unsafe { hr_face_reference_table(face, tag) };
        let mut len = 0;
        let data = unsafe { hr_blob_get_data(blob, &raw mut len) };
        let Some(left) = budget.checked_sub(len as usize) else {
            unsafe { hr_blob_destroy(blob) };
            return None;
        };
        budget = left;
        if len != 0 && !data.is_null() {
            tables.push((
                tag,
                unsafe { core::slice::from_raw_parts(data.cast::<u8>(), len as usize) }.to_vec(),
            ));
        }
        unsafe { hr_blob_destroy(blob) };
    }
    if tables.is_empty() {
        return None;
    }
    tables.sort_by_key(|(tag, _)| *tag);
    let count = tables.len() as u16;
    let mut output = vec![0; 12 + 16 * tables.len()];
    let version = if tables.iter().any(|(tag, _)| {
        *tag == u32::from_be_bytes(*b"CFF ") || *tag == u32::from_be_bytes(*b"CFF2")
    }) {
        u32::from_be_bytes(*b"OTTO")
    } else {
        0x0001_0000
    };
    output[..4].copy_from_slice(&version.to_be_bytes());
    output[4..6].copy_from_slice(&count.to_be_bytes());
    let selector = count.ilog2() as u16;
    let search = (1u16 << selector) * 16;
    output[6..8].copy_from_slice(&search.to_be_bytes());
    output[8..10].copy_from_slice(&selector.to_be_bytes());
    output[10..12].copy_from_slice(&(count * 16 - search).to_be_bytes());
    for (i, (tag, data)) in tables.iter().enumerate() {
        let at = 12 + 16 * i;
        let offset = u32::try_from(output.len()).ok()?;
        let length = u32::try_from(data.len()).ok()?;
        let checksum = data.chunks(4).fold(0u32, |sum, chunk| {
            let mut b = [0; 4];
            b[..chunk.len()].copy_from_slice(chunk);
            sum.wrapping_add(u32::from_be_bytes(b))
        });
        for (pos, value) in [
            (at, *tag),
            (at + 4, checksum),
            (at + 8, offset),
            (at + 12, length),
        ] {
            output[pos..pos + 4].copy_from_slice(&value.to_be_bytes());
        }
        output.extend_from_slice(data);
        output.resize((output.len() + 3) & !3, 0);
    }
    Some(output)
}
pub(crate) unsafe fn coords(font: *mut hr_font_t) -> Vec<F2Dot14> {
    let mut len = 0;
    let data = unsafe { hr_font_get_var_coords_normalized(font, &raw mut len) };
    if len == 0 || data.is_null() {
        return Vec::new();
    }
    unsafe { core::slice::from_raw_parts(data, len as usize) }
        .iter()
        .map(|v| F2Dot14::from_bits(*v as i16))
        .collect()
}
pub(crate) unsafe fn scale(font: *mut hr_font_t) -> (f32, f32) {
    let mut x = 0;
    let mut y = 0;
    unsafe { hr_font_get_scale(font, &raw mut x, &raw mut y) };
    let upem = unsafe { hr_face_get_upem(hr_font_get_face(font)) }.max(1) as f32;
    (x as f32 / upem, y as f32 / upem)
}

pub(crate) unsafe fn reject_data(data: *mut c_void, destroy: hr_destroy_func_t) {
    if let Some(destroy) = destroy {
        unsafe { destroy(data) }
    }
}

// A parent callback draws in the parent's scale. Forward each segment at the
// child's scale, retaining correct path state at the public callback boundary.
pub(crate) struct Pen {
    pub(crate) funcs: *mut hr_draw_funcs_t,
    pub(crate) data: *mut c_void,
    pub(crate) state: hr_draw_state_t,
    pub(crate) x: f32,
    pub(crate) y: f32,
    pub(crate) start: Option<(f32, f32)>,
    pub(crate) current: (f32, f32),
}
impl skrifa::outline::OutlinePen for Pen {
    fn move_to(&mut self, x: f32, y: f32) {
        self.close();
        self.current = (x, y);
        unsafe {
            hr_draw_move_to(
                self.funcs,
                self.data,
                &raw mut self.state,
                x * self.x,
                y * self.y,
            );
        }
    }
    fn line_to(&mut self, x: f32, y: f32) {
        self.start.get_or_insert(self.current);
        self.current = (x, y);
        unsafe {
            hr_draw_line_to(
                self.funcs,
                self.data,
                &raw mut self.state,
                x * self.x,
                y * self.y,
            );
        }
    }
    fn quad_to(&mut self, a: f32, b: f32, x: f32, y: f32) {
        self.start.get_or_insert(self.current);
        self.current = (x, y);
        unsafe {
            hr_draw_quadratic_to(
                self.funcs,
                self.data,
                &raw mut self.state,
                a * self.x,
                b * self.y,
                x * self.x,
                y * self.y,
            );
        }
    }
    fn curve_to(&mut self, a: f32, b: f32, c: f32, d: f32, x: f32, y: f32) {
        self.start.get_or_insert(self.current);
        self.current = (x, y);
        unsafe {
            hr_draw_cubic_to(
                self.funcs,
                self.data,
                &raw mut self.state,
                a * self.x,
                b * self.y,
                c * self.x,
                d * self.y,
                x * self.x,
                y * self.y,
            );
        }
    }
    #[allow(clippy::float_cmp)] // Preserve contour topology even when an axis scale is zero.
    fn close(&mut self) {
        if let Some((x, y)) = self.start.take() {
            if self.current != (x, y) {
                unsafe {
                    hr_draw_line_to(
                        self.funcs,
                        self.data,
                        &raw mut self.state,
                        x * self.x,
                        y * self.y,
                    );
                }
            }
        }
        unsafe { hr_draw_close_path(self.funcs, self.data, &raw mut self.state) }
    }
}
impl Drop for Pen {
    fn drop(&mut self) {
        skrifa::outline::OutlinePen::close(self);
    }
}
