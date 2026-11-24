//! Fonts: a face together with a size and variation settings. Mirrors the
//! shaping-relevant half of HarfBuzz's `hb-font.h`.

use core::ffi::{c_char, c_int, c_uint, c_void};
use std::sync::{Arc, OnceLock};

use harfrust::{
    font::{name::GlyphName, Font, NormalizedCoord, Variation},
    ShaperFont,
};
use read_fonts::TableProvider;

use crate::common::{hr_bool_t, hr_codepoint_t, hr_direction_t, hr_position_t, hr_variation_t};
use crate::common::{hr_glyph_extents_t, ot_round};
use crate::face::hr_face_t;
use crate::font_funcs::{hr_font_funcs_t, Answer};
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};

/// The `font_data` a caller attached along with a set of callbacks.
///
/// Reference counted so detaching it and releasing its ownership are separate
/// operations. Sub-fonts inherit it through their reference to the parent font.
pub(crate) struct FontData {
    pub(crate) data: *mut c_void,
    destroy: hr_destroy_func_t,
}

// SAFETY: `hr_font_set_funcs` documents that the callbacks and their data must
// be safe to use, and to release, from any thread.
unsafe impl Send for FontData {}
unsafe impl Sync for FontData {}

impl Drop for FontData {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            // SAFETY: `destroy` was supplied alongside `data`.
            unsafe { destroy(self.data) };
        }
    }
}

/// A prepared font view and the stable allocation it borrows.
///
/// Keeping these in one private owner makes replacing the instance drop the
/// font view first. The explicit `Drop` preserves that invariant if the fields
/// are ever reordered.
struct PreparedFont {
    shaper: Option<ShaperFont<'static, 'static>>,
    instance: Arc<Font>,
}

impl PreparedFont {
    fn new(instance: Font) -> Self {
        let instance = Arc::new(instance);
        let shaper = ShaperFont::new(&instance);
        // SAFETY: the prepared font borrows the shared allocation owned by
        // `instance`. Moving `Arc` does not retag the pointee as unique, and
        // `Drop` clears the borrow before `instance` is released.
        let shaper = Some(unsafe {
            core::mem::transmute::<ShaperFont<'_, '_>, ShaperFont<'static, 'static>>(shaper)
        });
        Self { shaper, instance }
    }

    fn shaper(&self) -> Option<&ShaperFont<'static, 'static>> {
        self.shaper.as_ref()
    }
}

impl Drop for PreparedFont {
    fn drop(&mut self) {
        self.shaper = None;
    }
}

/// A font: a face with a scale, optional pixel and point sizes, and variation
/// settings applied.
pub struct hr_font_t {
    header: ObjectHeader,
    /// Owned reference to the face this font draws from.
    face: *mut hr_face_t,
    /// `None` only for the immortal empty font.
    prepared: Option<PreparedFont>,
    pub(crate) x_scale: c_int,
    pub(crate) y_scale: c_int,
    pub(crate) x_ppem: c_uint,
    pub(crate) y_ppem: c_uint,
    pub(crate) ptem: f32,
    /// Normalized coordinates in HarfBuzz's representation: 2.14 values widened
    /// to `int`, so `hr_font_get_var_coords_normalized` can hand back a
    /// pointer directly.
    coords: Vec<c_int>,
    /// Owned reference to the callbacks, or null for the built-in ones.
    pub(crate) funcs: *mut hr_font_funcs_t,
    pub(crate) font_data: Option<Arc<FontData>>,
    /// Owned reference to the parent, for fonts made by
    /// [`hr_font_create_sub_font`]. Callbacks this font does not carry are
    /// answered by the parent, so this is walked on every dispatch.
    pub(crate) parent: *mut hr_font_t,
}

impl hr_font_t {
    /// Whether this font, or any it descends from, carries callbacks.
    ///
    /// A sub-font answers with its parent's callbacks, so the adapter has to
    /// be installed for it even when it carries none of its own.
    pub(crate) fn has_callbacks(&self) -> bool {
        let mut font = self;
        loop {
            // The built-in callbacks are what shaping does anyway, so a chain
            // carrying only those needs nothing installed over it.
            if !font.funcs.is_null() && font.funcs != crate::font_funcs::builtin_funcs() {
                return true;
            }
            // SAFETY: each font owns its reference to its parent.
            match unsafe { font.parent.as_ref() } {
                Some(parent) => font = parent,
                None => return false,
            }
        }
    }

    /// The glyph the font's own tables give for a character.
    ///
    /// This goes through the same charmap shaping uses, which knows about the
    /// legacy cmap subtables -- Macintosh Roman, and the Windows symbol
    /// encoding's private-use pages -- so that a glyph shaping can find is one
    /// this reports.
    pub(crate) fn builtin_nominal_glyph(&self, unicode: u32) -> Option<hr_codepoint_t> {
        self.prepared
            .as_ref()?
            .shaper()?
            .default_nominal_glyph(unicode)
            .map(|glyph| glyph.to_u32())
    }

    /// As [`hr_font_t::builtin_nominal_glyph`], for a variation selector.
    pub(crate) fn builtin_variation_glyph(
        &self,
        unicode: u32,
        variation_selector: u32,
    ) -> Option<hr_codepoint_t> {
        self.prepared
            .as_ref()?
            .shaper()?
            .default_variation_glyph(unicode, variation_selector)
            .map(|glyph| glyph.to_u32())
    }

    /// What the font's own tables say a glyph's extents are.
    pub(crate) fn builtin_extents(&self, glyph: hr_codepoint_t) -> Option<hr_glyph_extents_t> {
        let extents = self
            .builtin_shaping_font()?
            .default_glyph_extents(harfrust::GlyphId::from(glyph))?;
        Some(hr_glyph_extents_t {
            x_bearing: extents.x_bearing,
            y_bearing: extents.y_bearing,
            width: extents.width,
            height: extents.height,
        })
    }

    /// This face's design units, which everything the tables say is in.
    pub(crate) fn upem(&self) -> c_int {
        // SAFETY: a font owns its reference to its face.
        unsafe { self.face.as_ref() }.map_or(1000, |face| face.upem() as c_int)
    }

    fn builtin_shaping_font(&self) -> Option<&ShaperFont<'static, 'static>> {
        self.shaper()
    }

    /// How far the font's own tables advance a glyph, in reported units.
    pub(crate) fn builtin_h_advance(&self, glyph: hr_codepoint_t) -> i32 {
        let Some(font) = self.builtin_shaping_font() else {
            return 0;
        };
        font.default_glyph_h_advance(harfrust::GlyphId::from(glyph))
    }

    /// As [`hr_font_t::builtin_h_advance`], downwards.
    pub(crate) fn builtin_v_advance(&self, glyph: hr_codepoint_t) -> i32 {
        let Some(font) = self.builtin_shaping_font() else {
            return 0;
        };
        font.default_glyph_v_advance(harfrust::GlyphId::from(glyph))
    }

    /// Where the font's own tables hang a glyph from, in reported units.
    pub(crate) fn builtin_v_origin(&self, glyph: hr_codepoint_t) -> Option<(i32, i32)> {
        Some(
            self.builtin_shaping_font()?
                .default_glyph_v_origin(harfrust::GlyphId::from(glyph)),
        )
    }

    /// How far a glyph advances, through whichever callback answers.
    pub(crate) fn glyph_h_advance(&self, font: *mut hr_font_t, glyph: hr_codepoint_t) -> i32 {
        match crate::font_funcs::FontFuncsAdapter::new(font, self).call_h_advance(glyph) {
            Answer::Builtin => self.builtin_h_advance(glyph),
            Answer::Value(advance) => advance,
        }
    }

    /// As [`hr_font_t::glyph_h_advance`], downwards.
    pub(crate) fn glyph_v_advance(&self, font: *mut hr_font_t, glyph: hr_codepoint_t) -> i32 {
        match crate::font_funcs::FontFuncsAdapter::new(font, self).call_v_advance(glyph) {
            Answer::Builtin => self.builtin_v_advance(glyph),
            Answer::Value(advance) => advance,
        }
    }

    /// Where a glyph hangs from, through whichever callback answers.
    pub(crate) fn glyph_v_origin(
        &self,
        font: *mut hr_font_t,
        glyph: hr_codepoint_t,
    ) -> Option<(i32, i32)> {
        match crate::font_funcs::FontFuncsAdapter::new(font, self).call_v_origin(glyph) {
            Answer::Builtin => self.builtin_v_origin(glyph),
            Answer::Value(origin) => origin,
        }
    }

    /// The name the face gives a glyph, if it names it at all.
    pub(crate) fn glyph_name(&self, glyph: hr_codepoint_t) -> Option<GlyphName> {
        let shaper = self.prepared.as_ref()?.shaper()?;
        shaper.glyph_name(glyph.into())
    }

    /// A glyph's extents, from whichever callback answers, or from the font's
    /// own tables when none is installed. `None` when nothing can say.
    pub(crate) fn glyph_extents(
        &self,
        font: *mut hr_font_t,
        glyph: hr_codepoint_t,
    ) -> Option<hr_glyph_extents_t> {
        match crate::font_funcs::FontFuncsAdapter::new(font, self).call_extents(glyph)? {
            Answer::Builtin => self.builtin_extents(glyph),
            Answer::Value(extents) => Some(extents),
        }
    }

    /// The data this font's own callbacks were given.
    pub(crate) fn callback_data(&self) -> *mut c_void {
        self.font_data
            .as_ref()
            .map_or(core::ptr::null_mut(), |data| data.data)
    }

    pub(crate) fn face(&self) -> *mut hr_face_t {
        self.face
    }

    pub(crate) fn instance(&self) -> Option<&Font> {
        self.prepared.as_ref().map(|prepared| &*prepared.instance)
    }

    pub(crate) fn shaper(&self) -> Option<&ShaperFont<'static, 'static>> {
        self.prepared.as_ref().and_then(PreparedFont::shaper)
    }

    /// Rebuilds the font instance after a change to variation settings, and
    /// refreshes the normalized coordinate mirror.
    fn set_instance(&mut self, instance: Font) {
        let coords = instance
            .normalized_coords()
            .iter()
            .map(|coord| c_int::from(coord.to_bits()))
            .collect();
        let mut prepared = PreparedFont::new(instance);
        if let Some(shaper) = &mut prepared.shaper {
            shaper.set_scale_separate(self.x_scale, self.y_scale);
        }
        self.coords = coords;
        self.prepared = Some(prepared);
    }
}

impl Drop for hr_font_t {
    fn drop(&mut self) {
        self.prepared = None;
        // SAFETY: this font owns one reference to each of these.
        unsafe {
            object::destroy(self.funcs);
            object::destroy(self.parent);
            object::destroy(self.face);
        }
    }
}

static EMPTY_FONT: OnceLock<Empty<hr_font_t>> = OnceLock::new();

impl Object for hr_font_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    unsafe fn finalize(ptr: *mut Self) {
        // Detach before running caller code, without keeping an exclusive
        // reference to the font across a callback that may query it. Funcs,
        // parent, face and the prepared view remain alive until Box::drop.
        let data = unsafe { (&raw mut (*ptr).font_data).replace(None) };
        drop(data);
    }

    fn empty() -> *mut Self {
        EMPTY_FONT
            .get_or_init(|| {
                Empty::new(hr_font_t {
                    header: ObjectHeader::immortal(),
                    face: hr_face_t::empty(),
                    prepared: None,
                    x_scale: 0,
                    y_scale: 0,
                    x_ppem: 0,
                    y_ppem: 0,
                    ptem: 0.0,
                    coords: Vec::new(),
                    funcs: core::ptr::null_mut(),
                    font_data: None,
                    parent: core::ptr::null_mut(),
                })
            })
            .get()
    }
}

/// Creates a font over a face.
///
/// The font takes its own reference to the face. Its scale starts at the
/// face's units per em, so positions come out in font units until
/// [`hr_font_set_scale`] says otherwise.
///
/// Never returns `NULL`; a face with no font data yields the empty font.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_font_create(face: *mut hr_face_t) -> *mut hr_font_t {
    let Some(face_ref) = (unsafe { face.as_ref() }) else {
        return hr_font_t::empty();
    };
    let Some(font) = face_ref.font() else {
        return hr_font_t::empty();
    };
    let upem = font
        .tables()
        .head()
        .map_or(1000, |head| c_int::from(head.units_per_em()));
    let owned_face = unsafe { object::reference(face) };
    let mut this = hr_font_t {
        header: ObjectHeader::new(),
        face: owned_face,
        prepared: None,
        x_scale: upem,
        y_scale: upem,
        x_ppem: 0,
        y_ppem: 0,
        ptem: 0.0,
        coords: Vec::new(),
        funcs: core::ptr::null_mut(),
        font_data: None,
        parent: core::ptr::null_mut(),
    };
    this.set_instance(font.default_instance());
    object::create(this)
}

/// Creates a font that starts out as a copy of `parent`.
///
/// The sub-font holds a reference to its parent and inherits its scale, pixel
/// and point sizes, variation settings and callbacks; changing it afterwards
/// does not affect the parent.
///
/// # Safety
///
/// `parent` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_create_sub_font(parent: *mut hr_font_t) -> *mut hr_font_t {
    let Some(parent_ref) = (unsafe { parent.as_ref() }) else {
        return hr_font_t::empty();
    };
    let Some(instance) = parent_ref.instance() else {
        return hr_font_t::empty();
    };
    let mut this = hr_font_t {
        header: ObjectHeader::new(),
        face: unsafe { object::reference(parent_ref.face) },
        prepared: None,
        x_scale: parent_ref.x_scale,
        y_scale: parent_ref.y_scale,
        x_ppem: parent_ref.x_ppem,
        y_ppem: parent_ref.y_ppem,
        ptem: parent_ref.ptem,
        coords: Vec::new(),
        // Nothing of its own: a sub-font answers with whatever its parent
        // answers with at the time it is asked, so that callbacks installed
        // on the parent afterwards are its callbacks too. Copying them here
        // would freeze the parent as it was.
        funcs: core::ptr::null_mut(),
        font_data: None,
        parent: unsafe { object::reference(parent) },
    };
    this.set_instance(
        instance
            .instance_builder()
            .normalized_coords(instance.normalized_coords().iter().copied())
            .build(),
    );
    object::create(this)
}

/// Returns the immortal empty font.
#[no_mangle]
pub extern "C" fn hr_font_get_empty() -> *mut hr_font_t {
    hr_font_t::empty()
}

/// Increments a font's reference count.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_reference(font: *mut hr_font_t) -> *mut hr_font_t {
    unsafe { object::reference(font) }
}

/// Decrements a font's reference count, freeing it at zero.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and the caller must own the
/// reference being released.
#[no_mangle]
pub unsafe extern "C" fn hr_font_destroy(font: *mut hr_font_t) {
    unsafe { object::destroy(font) };
}

/// Attaches user data to a font.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_user_data(
    font: *mut hr_font_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(font, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data previously attached to a font.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_user_data(
    font: *mut hr_font_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(font, key) }
}

/// Marks a font immutable, and every font it descends from.
///
/// A sub-font answers with its parent's callbacks and scale at the time it is
/// asked, so it only holds still if its parent does. HarfBuzz freezes the
/// chain the same way.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_make_immutable(font: *mut hr_font_t) {
    let mut font = font;
    // SAFETY: each font owns its reference to its parent.
    while let Some(state) = unsafe { font.as_ref() } {
        unsafe { object::make_immutable(font) };
        font = state.parent;
    }
}

/// Returns whether a font has been marked immutable.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_is_immutable(font: *mut hr_font_t) -> hr_bool_t {
    unsafe { object::is_immutable(font.cast_const()) }.into()
}

/// Returns the face a font was created over, without taking a reference.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_face(font: *mut hr_font_t) -> *mut hr_face_t {
    unsafe { object::or_empty(font.cast_const()) }.face()
}

/// Returns a font's parent, or `NULL` if it is not a sub-font.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_parent(font: *mut hr_font_t) -> *mut hr_font_t {
    let parent = unsafe { object::or_empty(font.cast_const()) }.parent;
    // A font made from a face has no parent of its own, and answers with the
    // font that is empty rather than with nothing: a caller walking up the
    // chain has something to stop at.
    if parent.is_null() {
        return hr_font_t::empty();
    }
    parent
}

/// Sets a font's scale.
///
/// Positions are reported as `font_units * scale / upem`, so a scale equal to
/// the face's units per em leaves values in font units. For 26.6 fixed point
/// at a given pixel size, pass `size * 64`.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_scale(font: *mut hr_font_t, x_scale: c_int, y_scale: c_int) {
    if let Some(font) = unsafe { object::as_mutable(font) } {
        font.x_scale = x_scale;
        font.y_scale = y_scale;
        if let Some(shaper) = font
            .prepared
            .as_mut()
            .and_then(|prepared| prepared.shaper.as_mut())
        {
            shaper.set_scale_separate(x_scale, y_scale);
        }
    }
}

/// Returns a font's scale.
///
/// # Safety
///
/// `font` must be `NULL` or a live font; `x_scale` and `y_scale` must be
/// `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_scale(
    font: *mut hr_font_t,
    x_scale: *mut c_int,
    y_scale: *mut c_int,
) {
    let font = unsafe { object::or_empty(font.cast_const()) };
    if let Some(out) = unsafe { x_scale.as_mut() } {
        *out = font.x_scale;
    }
    if let Some(out) = unsafe { y_scale.as_mut() } {
        *out = font.y_scale;
    }
}

/// Sets a font's horizontal and vertical pixels per em. Zero leaves an axis
/// unset. `BASE` device adjustments use the ppem for their coordinate axis.
///
/// # Safety
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_ppem(font: *mut hr_font_t, x_ppem: c_uint, y_ppem: c_uint) {
    if let Some(font) = unsafe { object::as_mutable(font) } {
        font.x_ppem = x_ppem;
        font.y_ppem = y_ppem;
    }
}

/// Returns a font's horizontal and vertical pixels per em.
///
/// # Safety
/// `font` must be `NULL` or a live font; output pointers must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_ppem(
    font: *mut hr_font_t,
    x_ppem: *mut c_uint,
    y_ppem: *mut c_uint,
) {
    let font = unsafe { object::or_empty(font.cast_const()) };
    if let Some(out) = unsafe { x_ppem.as_mut() } {
        *out = font.x_ppem;
    }
    if let Some(out) = unsafe { y_ppem.as_mut() } {
        *out = font.y_ppem;
    }
}

/// Sets a font's point size, used when applying the `trak` table.
///
/// Pass zero to disable tracking.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_ptem(font: *mut hr_font_t, ptem: f32) {
    if let Some(font) = unsafe { object::as_mutable(font) } {
        font.ptem = ptem;
    }
}

/// Returns a font's point size, or zero if none is set.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_ptem(font: *mut hr_font_t) -> f32 {
    unsafe { object::or_empty(font.cast_const()) }.ptem
}

/// Sets a font's variation settings, in user space.
///
/// Axes that are not named are reset to their default values, so this
/// replaces any previous variation settings rather than adding to them.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `variations` must point to
/// `variations_length` readable entries.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_variations(
    font: *mut hr_font_t,
    variations: *const hr_variation_t,
    variations_length: c_uint,
) {
    let Some(font) = (unsafe { object::as_mutable(font) }) else {
        return;
    };
    let Some(instance) = font.instance() else {
        return;
    };
    let settings: Vec<Variation> = if variations.is_null() || variations_length == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller guarantees the array is readable.
        unsafe { core::slice::from_raw_parts(variations, variations_length as usize) }
            .iter()
            .map(|variation| {
                Variation::new(crate::common::tag_to_rust(variation.tag), variation.value)
            })
            .collect()
    };
    let rebuilt = instance.instance_builder().variations(settings).build();
    font.set_instance(rebuilt);
}

/// Sets a font's variation settings from normalized coordinates.
///
/// Coordinates are 2.14 fixed point values in axis order. Missing axes take
/// their default value; extra coordinates are ignored.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `coords` must point to
/// `coords_length` readable entries.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_var_coords_normalized(
    font: *mut hr_font_t,
    coords: *const c_int,
    coords_length: c_uint,
) {
    let Some(font) = (unsafe { object::as_mutable(font) }) else {
        return;
    };
    let Some(instance) = font.instance() else {
        return;
    };
    let settings: Vec<NormalizedCoord> = if coords.is_null() || coords_length == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller guarantees the array is readable.
        unsafe { core::slice::from_raw_parts(coords, coords_length as usize) }
            .iter()
            .map(|coord| NormalizedCoord::from_bits(*coord as i16))
            .collect()
    };
    let rebuilt = instance
        .instance_builder()
        .normalized_coords(settings)
        .build();
    font.set_instance(rebuilt);
}

/// Returns a font's normalized coordinates, writing their count to `length`.
///
/// The returned pointer stays valid until the font's variation settings change
/// or the font is freed.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `length` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_var_coords_normalized(
    font: *mut hr_font_t,
    length: *mut c_uint,
) -> *const c_int {
    let font = unsafe { object::or_empty(font.cast_const()) };
    if let Some(out) = unsafe { length.as_mut() } {
        *out = font.coords.len() as c_uint;
    }
    font.coords.as_ptr()
}

/// Sets a font's variation settings from a named instance in the `fvar` table.
///
/// An index that does not exist resets the settings to their defaults.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_var_named_instance(font: *mut hr_font_t, instance: c_uint) {
    let Some(font) = (unsafe { object::as_mutable(font) }) else {
        return;
    };
    let Some(current) = font.instance() else {
        return;
    };
    let rebuilt = current.instance_builder().named_instance(instance).build();
    font.set_instance(rebuilt);
}

/// Sets the callbacks a font uses during shaping.
///
/// The font takes a reference to `ffuncs` and takes ownership of `font_data`,
/// releasing it through `destroy` when the callbacks are replaced or the font
/// is freed. Passing `NULL` for `ffuncs` asks for no callbacks at all, which
/// answers nothing; [`hr_ot_font_set_funcs`] is the way back to the built-in
/// ones.
///
/// As in HarfBuzz, an installed funcs object is authoritative: it is not
/// blended with the built-in callbacks, and any callback it leaves unset
/// reports nothing available, giving glyph 0, a zero advance and no extents.
/// Populate every callback you need before installing the object.
///
/// Callbacks are handed the font they were installed on so they can read its
/// scale and variation settings. They must not modify it, nor free it, while
/// shaping is under way; [`hr_font_make_immutable`] is a convenient way to
/// guarantee that.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, `ffuncs` must be `NULL` or live, and
/// the callbacks must be safe to call with `font_data` from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_font_set_funcs(
    font: *mut hr_font_t,
    ffuncs: *mut hr_font_funcs_t,
    font_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let Some(font) = (unsafe { object::as_mutable(font) }) else {
        if let Some(destroy) = destroy {
            unsafe { destroy(font_data) };
        }
        return;
    };
    let previous = font.funcs;
    // No callbacks is not the same as never having set any: HarfBuzz reads
    // NULL as its empty funcs, which answer nothing, where a font that was
    // never given callbacks reads its own tables.
    let ffuncs = if ffuncs.is_null() {
        hr_font_funcs_t::empty()
    } else {
        ffuncs
    };
    font.funcs = unsafe { object::reference(ffuncs) };
    // Dropping the old data runs its destroy callback, unless a sub-font still
    // holds a reference to it. That happens before the callbacks it was given
    // with are released, as in HarfBuzz.
    let previous_data = font.font_data.replace(Arc::new(FontData {
        data: font_data,
        destroy,
    }));
    drop(previous_data);
    unsafe { object::destroy(previous) };
}

/// Installs the built-in callbacks, which read the font's own tables.
///
/// A font starts out answering this way, and this puts it back after
/// [`hr_font_set_funcs`] has installed others. HarfBuzz spells it
/// `hb_ot_font_set_funcs`, and it is the only way back there too: asking
/// `hr_font_set_funcs` for no callbacks means no callbacks, not these.
///
/// Any data the replaced callbacks were given is released, as it is when they
/// are replaced by other callbacks.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_ot_font_set_funcs(font: *mut hr_font_t) {
    let Some(font) = (unsafe { object::as_mutable(font) }) else {
        return;
    };
    let previous = font.funcs;
    // Said outright, rather than by carrying nothing: a sub-font carrying
    // nothing asks its parent, and this is a font that does not.
    font.funcs = unsafe { object::reference(crate::font_funcs::builtin_funcs()) };
    let previous_data = font.font_data.take();
    drop(previous_data);
    unsafe { object::destroy(previous) };
}

/// Maps a Unicode scalar value to a glyph, returning false if the font has
/// none.
///
/// Callbacks set by [`hr_font_set_funcs`] answer this, as they answer for the
/// font while shaping. Only a font that was never given any reads the font's
/// own `cmap`.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `glyph` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_nominal_glyph(
    font: *mut hr_font_t,
    unicode: hr_codepoint_t,
    glyph: *mut hr_codepoint_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    // The glyph is reported as not found before anything is asked, so a
    // caller that ignores the return value reads 0 rather than whatever it
    // happened to pass in. HarfBuzz writes it the same way.
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = 0;
    }
    let found =
        match crate::font_funcs::FontFuncsAdapter::new(font, state).call_nominal_glyph(unicode) {
            // Callbacks are installed somewhere in the chain, and none of them
            // answer for this.
            None => None,
            Some(Answer::Value(glyph)) => Some(glyph),
            // Nobody carries callbacks, so the font's own tables answer.
            Some(Answer::Builtin) => state.builtin_nominal_glyph(unicode),
        };
    let Some(found) = found else {
        return false.into();
    };
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = found;
    }
    true.into()
}

/// Maps a strided batch of Unicode codepoints to nominal glyphs.
///
/// Stops at the first missing glyph and returns the number mapped. Zero
/// strides may be used to read or write the same location repeatedly.
///
/// # Safety
/// `font` must be `NULL` or live. The arrays must be valid for `count`
/// entries at their respective byte strides.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_nominal_glyphs(
    font: *mut hr_font_t,
    count: c_uint,
    first_unicode: *const hr_codepoint_t,
    unicode_stride: c_uint,
    first_glyph: *mut hr_codepoint_t,
    glyph_stride: c_uint,
) -> c_uint {
    if count == 0 || first_unicode.is_null() || first_glyph.is_null() {
        return 0;
    }
    let state = unsafe { object::or_empty(font.cast_const()) };
    unsafe {
        crate::font_funcs::FontFuncsAdapter::new(font, state).call_nominal_glyphs(
            count,
            first_unicode,
            unicode_stride,
            first_glyph,
            glyph_stride,
        )
    }
}

/// Maps a Unicode scalar value and variation selector to a glyph, returning
/// false if the font has none.
///
/// Callbacks set by [`hr_font_set_funcs`] answer this, as they answer for the
/// font while shaping. Only a font that was never given any reads the font's
/// own `cmap`.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `glyph` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_variation_glyph(
    font: *mut hr_font_t,
    unicode: hr_codepoint_t,
    variation_selector: hr_codepoint_t,
    glyph: *mut hr_codepoint_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    // As in `hr_font_get_nominal_glyph`, not found until it is.
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = 0;
    }
    let found = match crate::font_funcs::FontFuncsAdapter::new(font, state)
        .call_variation_glyph(unicode, variation_selector)
    {
        None => None,
        Some(Answer::Value(glyph)) => Some(glyph),
        Some(Answer::Builtin) => state.builtin_variation_glyph(unicode, variation_selector),
    };
    let Some(found) = found else {
        return false.into();
    };
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = found;
    }
    true.into()
}

/// Maps a Unicode scalar value to a glyph, with or without a variation
/// selector, returning false if the font has none.
///
/// A variation selector of zero asks for the plain mapping. A selector the
/// font has no glyph for falls back to the plain mapping, which is what
/// HarfBuzz does.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `glyph` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph(
    font: *mut hr_font_t,
    unicode: hr_codepoint_t,
    variation_selector: hr_codepoint_t,
    glyph: *mut hr_codepoint_t,
) -> hr_bool_t {
    if variation_selector != 0 {
        // SAFETY: the caller's guarantees carry through.
        let found =
            unsafe { hr_font_get_variation_glyph(font, unicode, variation_selector, glyph) };
        if found != 0 {
            return found;
        }
    }
    // SAFETY: as above.
    unsafe { hr_font_get_nominal_glyph(font, unicode, glyph) }
}

/// How far a glyph advances when text runs horizontally.
///
/// Callbacks set by [`hr_font_set_funcs`] answer this, as they answer for the
/// font while shaping. Only a font that was never given any reads the font's
/// own `hmtx`. The answer is in the units [`hr_font_set_scale`] asks for.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_h_advance(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    state.glyph_h_advance(font, glyph)
}

/// As [`hr_font_get_glyph_h_advance`], for text running vertically.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_v_advance(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    state.glyph_v_advance(font, glyph)
}

/// Fills in horizontal advances for a run of glyphs.
///
/// The glyphs and the advances are each read and written every `stride`
/// bytes, so a caller can walk its own structures rather than pack arrays.
///
/// # Safety
///
/// `font` must be `NULL` or a live font. `first_glyph` must be `NULL` or
/// point at `count` glyphs `glyph_stride` bytes apart, and `first_advance`
/// must be `NULL` or point at `count` writable advances `advance_stride`
/// bytes apart.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_h_advances(
    font: *mut hr_font_t,
    count: c_uint,
    first_glyph: *const hr_codepoint_t,
    glyph_stride: c_uint,
    first_advance: *mut hr_position_t,
    advance_stride: c_uint,
) {
    // SAFETY: the caller's guarantees carry through.
    unsafe {
        advances(
            font,
            count,
            first_glyph,
            glyph_stride,
            first_advance,
            advance_stride,
            true,
        );
    }
}

/// As [`hr_font_get_glyph_h_advances`], for text running vertically.
///
/// # Safety
///
/// As [`hr_font_get_glyph_h_advances`].
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_v_advances(
    font: *mut hr_font_t,
    count: c_uint,
    first_glyph: *const hr_codepoint_t,
    glyph_stride: c_uint,
    first_advance: *mut hr_position_t,
    advance_stride: c_uint,
) {
    // SAFETY: the caller's guarantees carry through.
    unsafe {
        advances(
            font,
            count,
            first_glyph,
            glyph_stride,
            first_advance,
            advance_stride,
            false,
        );
    }
}

/// The walk both advance runs share.
///
/// # Safety
///
/// As [`hr_font_get_glyph_h_advances`].
unsafe fn advances(
    font: *mut hr_font_t,
    count: c_uint,
    first_glyph: *const hr_codepoint_t,
    glyph_stride: c_uint,
    first_advance: *mut hr_position_t,
    advance_stride: c_uint,
    horizontal: bool,
) {
    if first_glyph.is_null() || first_advance.is_null() {
        return;
    }
    let state = unsafe { object::or_empty(font.cast_const()) };
    if horizontal
        && unsafe {
            crate::font_funcs::FontFuncsAdapter::new(font, state).call_h_advances(
                count,
                first_glyph,
                glyph_stride,
                first_advance,
                advance_stride,
            )
        }
    {
        return;
    }
    let mut glyph = first_glyph.cast::<u8>();
    let mut advance = first_advance.cast::<u8>();
    for _ in 0..count {
        // A stride can land anywhere, so nothing here assumes alignment.
        // SAFETY: the caller guarantees `count` glyphs a stride apart.
        let id = unsafe { glyph.cast::<hr_codepoint_t>().read_unaligned() };
        let value = if horizontal {
            state.glyph_h_advance(font, id)
        } else {
            state.glyph_v_advance(font, id)
        };
        // SAFETY: as above, and the advances are writable.
        unsafe { advance.cast::<hr_position_t>().write_unaligned(value) };
        // Use `wrapping_add` so the unused pointer formed on the final turn
        // does not step out of bounds under `ptr::add`.
        glyph = glyph.wrapping_add(glyph_stride as usize);
        advance = advance.wrapping_add(advance_stride as usize);
    }
}

/// Where a glyph hangs from when text runs horizontally.
///
/// Always the glyph's own origin, so this reports `0, 0` and true, which is
/// what HarfBuzz answers for a font reading its own tables.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `x` and `y` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_h_origin(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    let origin = crate::font_funcs::FontFuncsAdapter::new(font, state).call_h_origin(glyph);
    if let Some(out) = unsafe { x.as_mut() } {
        *out = origin.map_or(0, |origin| origin.0);
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = origin.map_or(0, |origin| origin.1);
    }
    origin.is_some().into()
}

/// Returns the horizontal kerning supplied by font callbacks, or zero.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_h_kerning(
    font: *mut hr_font_t,
    first_glyph: hr_codepoint_t,
    second_glyph: hr_codepoint_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    crate::font_funcs::FontFuncsAdapter::new(font, state).call_kerning(
        first_glyph,
        second_glyph,
        false,
    )
}

/// Returns the vertical kerning supplied by font callbacks, or zero.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_v_kerning(
    font: *mut hr_font_t,
    first_glyph: hr_codepoint_t,
    second_glyph: hr_codepoint_t,
) -> hr_position_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    crate::font_funcs::FontFuncsAdapter::new(font, state).call_kerning(
        first_glyph,
        second_glyph,
        true,
    )
}

/// Where a glyph hangs from when text runs vertically, returning false when
/// nothing can say.
///
/// Callbacks set by [`hr_font_set_funcs`] answer this, as they answer for the
/// font while shaping. Only a font that was never given any reads the font's
/// own `VORG` and `vmtx`.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `x` and `y` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_v_origin(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    // Nowhere until somewhere, so a caller ignoring the return value reads
    // zeroes rather than whatever it passed in.
    if let Some(out) = unsafe { x.as_mut() } {
        *out = 0;
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = 0;
    }
    let Some((origin_x, origin_y)) = state.glyph_v_origin(font, glyph) else {
        return false.into();
    };
    if let Some(out) = unsafe { x.as_mut() } {
        *out = origin_x;
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = origin_y;
    }
    true.into()
}

/// A glyph's ink extents, returning false when the font cannot say.
///
/// Callbacks set by [`hr_font_set_funcs`] answer this, as they answer for the
/// font while shaping. Only a font that was never given any reads the font's
/// own outlines. The answer is in the units [`hr_font_set_scale`] asks for.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `extents` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_extents(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    extents: *mut hr_glyph_extents_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    // Nothing until something, as above.
    if let Some(out) = unsafe { extents.as_mut() } {
        *out = hr_glyph_extents_t::default();
    }
    let Some(found) = state.glyph_extents(font, glyph) else {
        return false.into();
    };
    if let Some(out) = unsafe { extents.as_mut() } {
        *out = found;
    }
    true.into()
}

/// How far a glyph advances in the given direction: horizontally into `x`,
/// vertically into `y`, and zero into the other.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `x` and `y` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_advance_for_direction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) {
    let state = unsafe { object::or_empty(font.cast_const()) };
    let (dx, dy) = if crate::common::hr_direction_is_horizontal(direction) != 0 {
        (state.glyph_h_advance(font, glyph), 0)
    } else {
        (0, state.glyph_v_advance(font, glyph))
    };
    if let Some(out) = unsafe { x.as_mut() } {
        *out = dx;
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = dy;
    }
}

/// As [`hr_font_get_glyph_advance_for_direction`], for a run of glyphs.
///
/// # Safety
///
/// As [`hr_font_get_glyph_h_advances`].
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_advances_for_direction(
    font: *mut hr_font_t,
    direction: hr_direction_t,
    count: c_uint,
    first_glyph: *const hr_codepoint_t,
    glyph_stride: c_uint,
    first_advance: *mut hr_position_t,
    advance_stride: c_uint,
) {
    let horizontal = crate::common::hr_direction_is_horizontal(direction) != 0;
    // SAFETY: the caller's guarantees carry through.
    unsafe {
        advances(
            font,
            count,
            first_glyph,
            glyph_stride,
            first_advance,
            advance_stride,
            horizontal,
        );
    }
}

/// The origin every direction-relative call is measured from.
///
/// # Safety
///
/// `font` must be `NULL` or a live font.
unsafe fn origin_for_direction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
) -> (hr_position_t, hr_position_t) {
    let state = unsafe { object::or_empty(font.cast_const()) };
    if crate::common::hr_direction_is_horizontal(direction) != 0 {
        return crate::font_funcs::FontFuncsAdapter::new(font, state)
            .call_h_origin(glyph)
            .unwrap_or((0, 0));
    }
    if let Some(origin) = state.glyph_v_origin(font, glyph) {
        return origin;
    }
    // Nothing says where it hangs from vertically, so HarfBuzz guesses from
    // the horizontal origin: the middle of the advance, at the ascender.
    let x = state.glyph_h_advance(font, glyph) / 2;
    let y = state
        .instance()
        .and_then(|instance| instance.metrics().hhea_line)
        .map_or(0, |line| {
            ot_round(line.ascender.to_f64() * f64::from(state.y_scale) / f64::from(state.upem()))
                as i32
        });
    (x, y)
}

/// Where a glyph hangs from in the given direction.
///
/// A font that cannot say where a glyph hangs from vertically has one guessed
/// for it from the horizontal origin, as HarfBuzz does.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `x` and `y` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_origin_for_direction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) {
    let (origin_x, origin_y) = unsafe { origin_for_direction(font, glyph, direction) };
    if let Some(out) = unsafe { x.as_mut() } {
        *out = origin_x;
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = origin_y;
    }
}

/// Moves a point from the glyph's own origin to the direction's origin.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `x` and `y` must be `NULL` or
/// readable and writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_add_glyph_origin_for_direction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) {
    let (origin_x, origin_y) = unsafe { origin_for_direction(font, glyph, direction) };
    if let Some(out) = unsafe { x.as_mut() } {
        *out = out.saturating_add(origin_x);
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = out.saturating_add(origin_y);
    }
}

/// Moves a point from the direction's origin back to the glyph's own.
///
/// # Safety
///
/// As [`hr_font_add_glyph_origin_for_direction`].
#[no_mangle]
pub unsafe extern "C" fn hr_font_subtract_glyph_origin_for_direction(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    x: *mut hr_position_t,
    y: *mut hr_position_t,
) {
    let (origin_x, origin_y) = unsafe { origin_for_direction(font, glyph, direction) };
    if let Some(out) = unsafe { x.as_mut() } {
        *out = out.saturating_sub(origin_x);
    }
    if let Some(out) = unsafe { y.as_mut() } {
        *out = out.saturating_sub(origin_y);
    }
}

/// A glyph's ink extents, measured from the direction's origin rather than
/// from the glyph's own.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `extents` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_extents_for_origin(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    direction: hr_direction_t,
    extents: *mut hr_glyph_extents_t,
) -> hr_bool_t {
    // SAFETY: the caller's guarantees carry through.
    let found = unsafe { hr_font_get_glyph_extents(font, glyph, extents) };
    if found == 0 {
        return found;
    }
    let (origin_x, origin_y) = unsafe { origin_for_direction(font, glyph, direction) };
    if let Some(out) = unsafe { extents.as_mut() } {
        out.x_bearing = out.x_bearing.saturating_sub(origin_x);
        out.y_bearing = out.y_bearing.saturating_sub(origin_y);
    }
    true.into()
}

/// Writes as much of `bytes` as fits, always NUL-terminated.
///
/// # Safety
///
/// `out` must point at `size` writable bytes, and `size` must not be zero.
unsafe fn write_cstr(bytes: &[u8], out: *mut c_char, size: c_uint) {
    let take = bytes.len().min(size as usize - 1);
    // SAFETY: `take` is within both the source and the caller's buffer.
    unsafe {
        core::ptr::copy_nonoverlapping(bytes.as_ptr().cast::<c_char>(), out, take);
        *out.add(take) = 0;
    }
}

/// The name the face gives a glyph, returning false when it names none.
///
/// The name is written NUL-terminated, truncated to fit.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, and `name` must be `NULL` or point
/// at `size` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_name(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    name: *mut c_char,
    size: c_uint,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    if size != 0 {
        if let Some(out) = unsafe { name.as_mut() } {
            *out = 0;
        }
    }
    let Some(found) = state.glyph_name(glyph) else {
        return false.into();
    };
    if name.is_null() || size == 0 {
        // Nowhere to put it, but the face does name the glyph.
        return true.into();
    }
    // SAFETY: the caller guarantees `size` writable bytes.
    unsafe { write_cstr(found.as_str().as_bytes(), name, size) };
    true.into()
}

/// The glyph a face gives a name to, returning false when it names none such.
///
/// A length of -1 means the name is NUL-terminated.
///
/// # Safety
///
/// `font` must be `NULL` or a live font, `name` must be `NULL` or readable
/// for its length, and `glyph` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_font_get_glyph_from_name(
    font: *mut hr_font_t,
    name: *const c_char,
    len: c_int,
    glyph: *mut hr_codepoint_t,
) -> hr_bool_t {
    let state = unsafe { object::or_empty(font.cast_const()) };
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = 0;
    }
    let Some(wanted) = (unsafe { crate::common::str_from_raw(name, len) }) else {
        return false.into();
    };
    // The font indexes its stored names once and searches that afterwards.
    // Asking every glyph the face claims for its name instead would reread
    // the name source for each of a `MAXP` count nothing bounds.
    let found = state
        .instance()
        .and_then(|instance| instance.glyph_by_name(wanted))
        .map(|found| found.to_u32())
        // A glyph the face names nothing still answers to the `gidNNN` that
        // `hr_font_get_glyph_name` gives it.
        .or_else(|| {
            let candidate = wanted.strip_prefix("gid")?.parse().ok()?;
            state
                .glyph_name(candidate)
                .is_some_and(|name| name.as_str() == wanted)
                .then_some(candidate)
        });
    let Some(found) = found else {
        return false.into();
    };
    if let Some(out) = unsafe { glyph.as_mut() } {
        *out = found;
    }
    true.into()
}

/// The name a glyph goes by, falling back to `gidNNN` when the face names it
/// nothing.
///
/// # Safety
///
/// As [`hr_font_get_glyph_name`].
#[no_mangle]
pub unsafe extern "C" fn hr_font_glyph_to_string(
    font: *mut hr_font_t,
    glyph: hr_codepoint_t,
    s: *mut c_char,
    size: c_uint,
) {
    // SAFETY: the caller's guarantees carry through.
    if unsafe { hr_font_get_glyph_name(font, glyph, s, size) } != 0 {
        return;
    }
    if s.is_null() || size == 0 {
        return;
    }
    let text = format!("gid{glyph}");
    // SAFETY: as above.
    unsafe { write_cstr(text.as_bytes(), s, size) };
}

/// The glyph a string names: by the face's own names, by glyph number, by
/// `gidNNN`, or by `uniXXXX`.
///
/// # Safety
///
/// As [`hr_font_get_glyph_from_name`].
#[no_mangle]
pub unsafe extern "C" fn hr_font_glyph_from_string(
    font: *mut hr_font_t,
    s: *const c_char,
    len: c_int,
    glyph: *mut hr_codepoint_t,
) -> hr_bool_t {
    // SAFETY: the caller's guarantees carry through.
    if unsafe { hr_font_get_glyph_from_name(font, s, len, glyph) } != 0 {
        return true.into();
    }
    let Some(text) = (unsafe { crate::common::str_from_raw(s, len) }) else {
        return false.into();
    };
    let parsed = text
        .parse::<hr_codepoint_t>()
        .ok()
        .or_else(|| text.strip_prefix("gid")?.parse::<hr_codepoint_t>().ok());
    if let Some(found) = parsed {
        if let Some(out) = unsafe { glyph.as_mut() } {
            *out = found;
        }
        return true.into();
    }
    // `uniXXXX` names a character, and the font says which glyph that is.
    let Some(unicode) = text
        .strip_prefix("uni")
        .and_then(|hex| hr_codepoint_t::from_str_radix(hex, 16).ok())
    else {
        return false.into();
    };
    // SAFETY: as above.
    unsafe { hr_font_get_nominal_glyph(font, unicode, glyph) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::Cell;
    use core::ptr;

    struct Destruction {
        font: *mut hr_font_t,
        face: *mut hr_face_t,
        calls: Cell<u32>,
        queried_face: Cell<bool>,
        replacement_calls: Cell<u32>,
        replacement_queried_face: Cell<bool>,
        replacement_released_immediately: Cell<bool>,
        reset_rejected: Cell<bool>,
    }

    unsafe extern "C" fn release_replacement(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state
            .replacement_calls
            .set(state.replacement_calls.get() + 1);
        state
            .replacement_queried_face
            .set(unsafe { hr_font_get_face(state.font) == state.face });
    }

    unsafe extern "C" fn release_font_data(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state.calls.set(state.calls.get() + 1);
        state
            .queried_face
            .set(unsafe { hr_font_get_face(state.font) == state.face });
        // Finalization rejects replacement ownership and, as for immutable
        // fonts, immediately releases the supplied data outside any &mut.
        unsafe { hr_font_set_funcs(state.font, ptr::null_mut(), data, Some(release_replacement)) };
        state
            .replacement_released_immediately
            .set(state.replacement_calls.get() == 1);
        let funcs = unsafe { (*state.font).funcs };
        unsafe { hr_ot_font_set_funcs(state.font) };
        state
            .reset_rejected
            .set(unsafe { (*state.font).funcs } == funcs);
    }

    fn check_finalization(font: *mut hr_font_t, through_sub_font: bool) {
        let state = Destruction {
            font,
            face: unsafe { hr_font_get_face(font) },
            calls: Cell::new(0),
            queried_face: Cell::new(false),
            replacement_calls: Cell::new(0),
            replacement_queried_face: Cell::new(false),
            replacement_released_immediately: Cell::new(false),
            reset_rejected: Cell::new(false),
        };
        assert!(!state.face.is_null());
        unsafe {
            hr_font_set_funcs(
                font,
                ptr::null_mut(),
                ptr::from_ref(&state).cast_mut().cast(),
                Some(release_font_data),
            );
            let sub = through_sub_font.then(|| hr_font_create_sub_font(font));
            hr_font_destroy(font);
            if let Some(sub) = sub {
                assert_eq!(state.calls.get(), 0);
                hr_font_destroy(sub);
            }
        }
        assert_eq!(state.calls.get(), 1);
        assert!(state.queried_face.get());
        assert_eq!(state.replacement_calls.get(), 1);
        assert!(state.replacement_queried_face.get());
        assert!(state.replacement_released_immediately.get());
        assert!(state.reset_rejected.get());
    }

    #[test]
    fn isolated_font_data_destroy_can_query_the_font() {
        // Avoid PreparedFont's separate self-borrow so default Miri isolates
        // callback aliasing during font destruction.
        let font = object::create(hr_font_t {
            header: ObjectHeader::new(),
            face: hr_face_t::empty(),
            prepared: None,
            x_scale: 1000,
            y_scale: 1000,
            x_ppem: 0,
            y_ppem: 0,
            ptem: 0.0,
            coords: Vec::new(),
            funcs: ptr::null_mut(),
            font_data: None,
            parent: ptr::null_mut(),
        });
        check_finalization(font, false);
    }

    fn public_font() -> *mut hr_font_t {
        let bytes = include_bytes!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../harfrust/tests/fonts/rb_custom/LaBelleAurore.ttf"
        ));
        unsafe {
            let blob = crate::hr_blob_create(
                bytes.as_ptr().cast(),
                bytes.len() as c_uint,
                crate::HR_MEMORY_MODE_DUPLICATE,
                ptr::null_mut(),
                None,
            );
            let face = crate::hr_face_create(blob, 0);
            let font = hr_font_create(face);
            crate::hr_face_destroy(face);
            crate::hr_blob_destroy(blob);
            assert_ne!(font, hr_font_get_empty());
            font
        }
    }

    #[test]
    fn public_font_data_destroy_can_query_the_font() {
        check_finalization(public_font(), false);
    }

    #[test]
    fn public_font_data_destroy_can_query_parent_released_by_sub_font() {
        check_finalization(public_font(), true);
    }
}
