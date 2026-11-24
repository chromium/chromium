//! Buffers of text and of the glyphs shaping produces. Mirrors HarfBuzz's
//! `hb-buffer.h`.

use core::ffi::{c_char, c_int, c_uint, c_void};
use std::sync::OnceLock;

use harfrust::{
    Buffer, BufferFlags, ClusterLevel, ContentType, Direction, GlyphInfo, GlyphPosition,
    SerializeFlags,
};

use crate::common::{direction_from_rust, direction_to_rust, hr_direction_t, write_c_string};
use crate::common::{
    hr_bool_t, hr_codepoint_t, hr_language_t, hr_mask_t, hr_position_t, hr_script_t, hr_tag_t,
    language_from_rust, language_to_rust, script_from_rust, script_to_rust, HR_CODEPOINT_INVALID,
};
use crate::font::hr_font_t;
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};

const MAX_CONTEXT_CODEPOINTS: usize = 5;

/// What a buffer currently holds.
///
/// An integer typedef rather than an enumeration. A value arriving from C need
/// not be one of the ones named here, and a Rust enumeration holding an
/// unnamed value is undefined behaviour rather than merely wrong.
pub type hr_buffer_content_type_t = c_int;

/// Nothing, or contents that have been cleared.
pub const HR_BUFFER_CONTENT_TYPE_INVALID: hr_buffer_content_type_t = 0;
/// Input characters, ready to be shaped.
pub const HR_BUFFER_CONTENT_TYPE_UNICODE: hr_buffer_content_type_t = 1;
/// The glyphs shaping produced.
pub const HR_BUFFER_CONTENT_TYPE_GLYPHS: hr_buffer_content_type_t = 2;

/// Flags controlling how a buffer is shaped.
///
/// These are combined with a bitwise or, so this is an integer typedef rather
/// than an enumeration.
pub type hr_buffer_flags_t = u32;

/// No flags set.
pub const HR_BUFFER_FLAG_DEFAULT: hr_buffer_flags_t = 0x0000_0000u32;
/// The buffer holds the start of a paragraph.
pub const HR_BUFFER_FLAG_BOT: hr_buffer_flags_t = 0x0000_0001u32;
/// The buffer holds the end of a paragraph.
pub const HR_BUFFER_FLAG_EOT: hr_buffer_flags_t = 0x0000_0002u32;
/// Show default-ignorable characters using the font's own glyphs.
pub const HR_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES: hr_buffer_flags_t = 0x0000_0004u32;
/// Remove default-ignorable characters entirely.
pub const HR_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES: hr_buffer_flags_t = 0x0000_0008u32;
/// Do not insert a dotted circle around invalid sequences.
pub const HR_BUFFER_FLAG_DO_NOT_INSERT_DOTTED_CIRCLE: hr_buffer_flags_t = 0x0000_0010u32;
/// Verify the shaping result. Accepted but not yet acted on.
pub const HR_BUFFER_FLAG_VERIFY: hr_buffer_flags_t = 0x0000_0020u32;
/// Produce the unsafe-to-concat glyph flag, which costs extra work.
pub const HR_BUFFER_FLAG_PRODUCE_UNSAFE_TO_CONCAT: hr_buffer_flags_t = 0x0000_0040u32;
/// Produce the safe-to-insert-tatweel glyph flag.
pub const HR_BUFFER_FLAG_PRODUCE_SAFE_TO_INSERT_TATWEEL: hr_buffer_flags_t = 0x0000_0080u32;
/// Every flag defined above.
pub const HR_BUFFER_FLAG_DEFINED: hr_buffer_flags_t = 0x0000_00FFu32;

/// How clusters are merged during shaping.
///
/// An integer typedef rather than an enumeration. A value arriving from C need
/// not be one of the ones named here, and a Rust enumeration holding an
/// unnamed value is undefined behaviour rather than merely wrong.
pub type hr_buffer_cluster_level_t = c_int;

/// Merge clusters by grapheme, keeping cluster values monotonic.
pub const HR_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES: hr_buffer_cluster_level_t = 0;
/// Merge clusters by character, keeping cluster values monotonic.
pub const HR_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS: hr_buffer_cluster_level_t = 1;
/// Do not merge clusters, and do not keep cluster values monotonic.
pub const HR_BUFFER_CLUSTER_LEVEL_CHARACTERS: hr_buffer_cluster_level_t = 2;
/// Merge clusters by grapheme without keeping cluster values monotonic.
pub const HR_BUFFER_CLUSTER_LEVEL_GRAPHEMES: hr_buffer_cluster_level_t = 3;
/// What a buffer starts out with.
pub const HR_BUFFER_CLUSTER_LEVEL_DEFAULT: hr_buffer_cluster_level_t =
    HR_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES;

/// Flags attached to an individual glyph by shaping.
///
/// These are combined with a bitwise or, so this is an integer typedef rather
/// than an enumeration.
pub type hr_glyph_flags_t = u32;

/// Breaking the text at this cluster requires reshaping both sides.
pub const HR_GLYPH_FLAG_UNSAFE_TO_BREAK: hr_glyph_flags_t = 0x0000_0001u32;
/// Changing the text on one side of this cluster may change the other.
pub const HR_GLYPH_FLAG_UNSAFE_TO_CONCAT: hr_glyph_flags_t = 0x0000_0002u32;
/// A tatweel may be inserted before this cluster to elongate the run.
pub const HR_GLYPH_FLAG_SAFE_TO_INSERT_TATWEEL: hr_glyph_flags_t = 0x0000_0004u32;
/// Every flag defined above.
pub const HR_GLYPH_FLAG_DEFINED: hr_glyph_flags_t = 0x0000_0007u32;

/// The format `hr_buffer_serialize_glyphs` writes.
///
/// A tag, and so an integer typedef: `hr_buffer_serialize_format_from_string`
/// hands back whatever was asked for, named here or not, and a Rust
/// enumeration holding an unnamed value is undefined behaviour rather than
/// merely wrong.
pub type hr_buffer_serialize_format_t = hr_tag_t;

/// A human-readable one-line form.
pub const HR_BUFFER_SERIALIZE_FORMAT_TEXT: hr_buffer_serialize_format_t = 0x5445_5854u32;
/// JSON. Not supported by this library.
pub const HR_BUFFER_SERIALIZE_FORMAT_JSON: hr_buffer_serialize_format_t = 0x4A53_4F4Eu32;
/// An unrecognised format.
pub const HR_BUFFER_SERIALIZE_FORMAT_INVALID: hr_buffer_serialize_format_t = 0x0000_0000u32;

/// Flags controlling what `hr_buffer_serialize_glyphs` includes.
///
/// These are combined with a bitwise or, so this is an integer typedef rather
/// than an enumeration.
pub type hr_buffer_serialize_flags_t = u32;

/// Include everything.
pub const HR_BUFFER_SERIALIZE_FLAG_DEFAULT: hr_buffer_serialize_flags_t = 0x0000_0000u32;
/// Leave out cluster values.
pub const HR_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS: hr_buffer_serialize_flags_t = 0x0000_0001u32;
/// Leave out positions.
pub const HR_BUFFER_SERIALIZE_FLAG_NO_POSITIONS: hr_buffer_serialize_flags_t = 0x0000_0002u32;
/// Write glyph indices rather than names.
pub const HR_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES: hr_buffer_serialize_flags_t = 0x0000_0004u32;
/// Include each glyph's ink extents.
pub const HR_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS: hr_buffer_serialize_flags_t = 0x0000_0008u32;
/// Include each glyph's flags.
pub const HR_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS: hr_buffer_serialize_flags_t = 0x0000_0010u32;
/// Leave out advances, making offsets absolute.
pub const HR_BUFFER_SERIALIZE_FLAG_NO_ADVANCES: hr_buffer_serialize_flags_t = 0x0000_0020u32;
/// Every flag defined above.
pub const HR_BUFFER_SERIALIZE_FLAG_DEFINED: hr_buffer_serialize_flags_t = 0x0000_003Fu32;

/// One item in a buffer: an input character before shaping, a glyph after.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, Hash, Debug)]
pub struct hr_glyph_info_t {
    /// The input codepoint before shaping, the glyph index after.
    pub codepoint: hr_codepoint_t,
    /// Reserved for internal use.
    pub mask: hr_mask_t,
    /// Index into the input text of the cluster this item belongs to.
    pub cluster: u32,
    /// Reserved for internal use.
    pub var1: u32,
    /// Reserved for internal use.
    pub var2: u32,
}

/// The position of one glyph, relative to the current point.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, Hash, Debug)]
pub struct hr_glyph_position_t {
    /// How far the pen moves after this glyph, when setting horizontally.
    pub x_advance: hr_position_t,
    /// How far the pen moves after this glyph, when setting vertically.
    pub y_advance: hr_position_t,
    /// How far this glyph shifts on the x-axis before being drawn.
    pub x_offset: hr_position_t,
    /// How far this glyph shifts on the y-axis before being drawn.
    pub y_offset: hr_position_t,
    /// Reserved for internal use.
    pub var: u32,
}

// These are handed out as pointers straight into HarfRust's own storage, so
// their layouts have to agree exactly.
const _: () = assert!(size_of::<hr_glyph_info_t>() == size_of::<GlyphInfo>());
const _: () = assert!(align_of::<hr_glyph_info_t>() == align_of::<GlyphInfo>());
const _: () = assert!(size_of::<hr_glyph_position_t>() == size_of::<GlyphPosition>());
const _: () = assert!(align_of::<hr_glyph_position_t>() == align_of::<GlyphPosition>());

/// A `NULL`-terminated array of static C strings, safe to share across
/// threads because the strings it points at are immutable and never freed.
pub(crate) struct CStrArray<const N: usize>(pub(crate) [*const c_char; N]);

// SAFETY: the contents are pointers to string literals with static storage.
unsafe impl<const N: usize> Sync for CStrArray<N> {}

/// A buffer of text to shape, and of the glyphs shaping produces.
pub struct hr_buffer_t {
    header: ObjectHeader,
    pub(crate) buffer: Buffer,
}

static EMPTY_BUFFER: OnceLock<Empty<hr_buffer_t>> = OnceLock::new();

impl Object for hr_buffer_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_BUFFER
            .get_or_init(|| {
                Empty::new(hr_buffer_t {
                    header: ObjectHeader::immortal(),
                    buffer: Buffer::new(),
                })
            })
            .get()
    }
}

fn content_type_to_rust(value: hr_buffer_content_type_t) -> Option<ContentType> {
    match value {
        HR_BUFFER_CONTENT_TYPE_INVALID => None,
        HR_BUFFER_CONTENT_TYPE_UNICODE => Some(ContentType::Unicode),
        HR_BUFFER_CONTENT_TYPE_GLYPHS => Some(ContentType::Glyphs),
        // A value naming none of these says as little as the invalid one.
        _ => None,
    }
}

fn content_type_from_rust(value: Option<ContentType>) -> hr_buffer_content_type_t {
    match value {
        None => HR_BUFFER_CONTENT_TYPE_INVALID,
        Some(ContentType::Unicode) => HR_BUFFER_CONTENT_TYPE_UNICODE,
        Some(ContentType::Glyphs) => HR_BUFFER_CONTENT_TYPE_GLYPHS,
    }
}

fn cluster_level_to_rust(value: hr_buffer_cluster_level_t) -> ClusterLevel {
    match value {
        HR_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES => ClusterLevel::MonotoneGraphemes,
        HR_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS => ClusterLevel::MonotoneCharacters,
        HR_BUFFER_CLUSTER_LEVEL_CHARACTERS => ClusterLevel::Characters,
        HR_BUFFER_CLUSTER_LEVEL_GRAPHEMES => ClusterLevel::Graphemes,
        // A value naming none of these leaves the buffer as it starts.
        _ => ClusterLevel::MonotoneGraphemes,
    }
}

fn cluster_level_from_rust(value: ClusterLevel) -> hr_buffer_cluster_level_t {
    match value {
        ClusterLevel::MonotoneGraphemes => HR_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES,
        ClusterLevel::MonotoneCharacters => HR_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS,
        ClusterLevel::Characters => HR_BUFFER_CLUSTER_LEVEL_CHARACTERS,
        ClusterLevel::Graphemes => HR_BUFFER_CLUSTER_LEVEL_GRAPHEMES,
    }
}

/// Creates an empty buffer.
#[no_mangle]
pub extern "C" fn hr_buffer_create() -> *mut hr_buffer_t {
    object::create(hr_buffer_t {
        header: ObjectHeader::new(),
        buffer: Buffer::new(),
    })
}

/// Creates an empty buffer carrying the same properties as `src`.
///
/// # Safety
///
/// `src` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_create_similar(src: *mut hr_buffer_t) -> *mut hr_buffer_t {
    let created = hr_buffer_create();
    let (Some(src), Some(dst)) = (unsafe { src.as_ref() }, unsafe { created.as_mut() }) else {
        return created;
    };
    // How the source is configured, and not what it is holding or what it
    // says about the text: HarfBuzz leaves the direction, script and language
    // for the new buffer to be told or to guess.
    dst.buffer.set_flags(src.buffer.flags());
    dst.buffer.set_cluster_level(src.buffer.cluster_level());
    dst.buffer.set_invisible_glyph(src.buffer.invisible_glyph());
    dst.buffer
        .set_not_found_variation_selector_glyph(src.buffer.not_found_variation_selector_glyph());
    created
}

/// Returns the immortal empty buffer.
#[no_mangle]
pub extern "C" fn hr_buffer_get_empty() -> *mut hr_buffer_t {
    hr_buffer_t::empty()
}

/// Increments a buffer's reference count.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reference(buffer: *mut hr_buffer_t) -> *mut hr_buffer_t {
    unsafe { object::reference(buffer) }
}

/// Decrements a buffer's reference count, freeing it at zero.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and the caller must own the
/// reference being released.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_destroy(buffer: *mut hr_buffer_t) {
    unsafe { object::destroy(buffer) };
}

/// Attaches user data to a buffer.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_user_data(
    buffer: *mut hr_buffer_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(buffer, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data previously attached to a buffer.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_user_data(
    buffer: *mut hr_buffer_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(buffer, key) }
}

/// Clears a buffer's contents and resets its properties to their defaults.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reset(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.reset();
    }
}

/// Clears a buffer's contents, keeping its properties and its allocation.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_clear_contents(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.clear();
    }
}

/// Grows a buffer so it can hold at least `size` items without reallocating.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_pre_allocate(
    buffer: *mut hr_buffer_t,
    size: c_uint,
) -> hr_bool_t {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return false.into();
    };
    buffer.buffer.reserve(size as usize).into()
}

/// Returns whether every allocation on a buffer has succeeded.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_allocation_successful(buffer: *mut hr_buffer_t) -> hr_bool_t {
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .allocation_successful()
        .into()
}

/// Appends one codepoint to a buffer with the given cluster value.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add(
    buffer: *mut hr_buffer_t,
    codepoint: hr_codepoint_t,
    cluster: c_uint,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.push(codepoint, cluster);
        // Whatever followed the text before now follows something else, and
        // the caller has said nothing about what: HarfBuzz drops it here.
        buffer.buffer.set_post_context_codepoints(&[]);
    }
}

/// Resolves HarfBuzz's `item_offset` / `item_length` convention against a
/// sequence of `total` code units.
///
/// A negative length means "to the end". Returns the half-open item range.
fn item_range(total: usize, item_offset: c_uint, item_length: c_int) -> (usize, usize) {
    let start = (item_offset as usize).min(total);
    let end = if item_length < 0 {
        total
    } else {
        start.saturating_add(item_length as usize).min(total)
    };
    (start, end)
}

/// Appends decoded UTF-8 in one allocation, adjusting the relative cluster
/// values produced by `Buffer::push_str` to HarfBuzz's absolute offsets.
fn append_utf8(buffer: &mut Buffer, text: &str, cluster_offset: c_uint) {
    let old_len = buffer.len();
    buffer.push_str(text);
    if cluster_offset != 0 {
        for info in &mut buffer.glyph_infos_mut()[old_len..] {
            info.cluster = info.cluster.wrapping_add(cluster_offset);
        }
    }
}

// Decode just one codepoint at a time so context past HarfBuzz's five-codepoint
// limit is never scanned. Like hb_utf8_t::next, consume one byte per error.
fn next_utf8(bytes: &[u8], index: &mut usize) -> u32 {
    let first = bytes[*index];
    *index += 1;
    if first <= 0x7F {
        return first.into();
    }

    let continuation = |byte: u8| (byte & 0xC0) == 0x80;
    if (0xC2..=0xDF).contains(&first) {
        if *index < bytes.len() && continuation(bytes[*index]) {
            let codepoint = (u32::from(first & 0x1F) << 6) | u32::from(bytes[*index] & 0x3F);
            *index += 1;
            return codepoint;
        }
    } else if (0xE0..=0xEF).contains(&first) {
        if *index + 1 < bytes.len()
            && continuation(bytes[*index])
            && continuation(bytes[*index + 1])
        {
            let codepoint = (u32::from(first & 0x0F) << 12)
                | (u32::from(bytes[*index] & 0x3F) << 6)
                | u32::from(bytes[*index + 1] & 0x3F);
            if codepoint >= 0x800 && !(0xD800..=0xDFFF).contains(&codepoint) {
                *index += 2;
                return codepoint;
            }
        }
    } else if (0xF0..=0xF4).contains(&first)
        && *index + 2 < bytes.len()
        && continuation(bytes[*index])
        && continuation(bytes[*index + 1])
        && continuation(bytes[*index + 2])
    {
        let codepoint = (u32::from(first & 0x07) << 18)
            | (u32::from(bytes[*index] & 0x3F) << 12)
            | (u32::from(bytes[*index + 1] & 0x3F) << 6)
            | u32::from(bytes[*index + 2] & 0x3F);
        if (0x10000..=0x10_FFFF).contains(&codepoint) {
            *index += 3;
            return codepoint;
        }
    }

    char::REPLACEMENT_CHARACTER.into()
}

fn decode_pre_context_utf8(bytes: &[u8], codepoints: &mut [u32; MAX_CONTEXT_CODEPOINTS]) -> usize {
    let mut start = bytes.len();
    let mut len = 0;
    while start > 0 && len < codepoints.len() {
        let end = start;
        start -= 1;
        while start > 0 && bytes[start] & 0xC0 == 0x80 && end - start < 4 {
            start -= 1;
        }

        let mut next = start;
        let codepoint = next_utf8(&bytes[..end], &mut next);
        if next != end {
            start = end - 1;
        }
        codepoints[len] = codepoint;
        len += 1;
    }
    len
}

fn decode_post_context_utf8(bytes: &[u8], codepoints: &mut [u32; MAX_CONTEXT_CODEPOINTS]) -> usize {
    let mut index = 0;
    let mut len = 0;
    while index < bytes.len() && len < codepoints.len() {
        codepoints[len] = next_utf8(bytes, &mut index);
        len += 1;
    }
    len
}

fn set_pre_context_utf8(buffer: &mut Buffer, bytes: &[u8]) {
    let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
    let len = decode_pre_context_utf8(bytes, &mut codepoints);
    buffer.set_pre_context_codepoints(&codepoints[..len]);
}

fn set_post_context_utf8(buffer: &mut Buffer, bytes: &[u8]) {
    let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
    let len = decode_post_context_utf8(bytes, &mut codepoints);
    buffer.set_post_context_codepoints(&codepoints[..len]);
}

fn set_pre_context_utf16(buffer: &mut Buffer, units: &[u16]) {
    let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
    let mut start = units.len();
    for _ in 0..MAX_CONTEXT_CODEPOINTS {
        if start == 0 {
            break;
        }
        start -= 1;
        if (0xDC00..=0xDFFF).contains(&units[start])
            && start > 0
            && (0xD800..=0xDBFF).contains(&units[start - 1])
        {
            start -= 1;
        }
    }
    let mut len = 0;
    for decoded in char::decode_utf16(units[start..].iter().copied()) {
        codepoints[len] = decoded.unwrap_or(char::REPLACEMENT_CHARACTER) as u32;
        len += 1;
    }
    codepoints[..len].reverse();
    buffer.set_pre_context_codepoints(&codepoints[..len]);
}

fn set_post_context_utf16(buffer: &mut Buffer, units: &[u16]) {
    let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
    let mut len = 0;
    for decoded in char::decode_utf16(units.iter().copied()).take(MAX_CONTEXT_CODEPOINTS) {
        codepoints[len] = decoded.unwrap_or(char::REPLACEMENT_CHARACTER) as u32;
        len += 1;
    }
    buffer.set_post_context_codepoints(&codepoints[..len]);
}

/// Appends UTF-16 text. Offsets and clusters count 16-bit code units.
/// A negative `text_length` reads to a zero code unit; a negative
/// `item_length` selects the rest of the text.
///
/// # Safety
///
/// `text` must point to `text_length` readable code units, or be zero-terminated.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add_utf16(
    buffer: *mut hr_buffer_t,
    text: *const u16,
    text_length: c_int,
    item_offset: c_uint,
    item_length: c_int,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    if text.is_null() {
        return;
    }
    let len = if text_length < 0 {
        let mut len = 0;
        while unsafe { *text.add(len) } != 0 {
            len += 1;
        }
        len
    } else {
        text_length as usize
    };
    let units = unsafe { core::slice::from_raw_parts(text, len) };
    let (start, end) = item_range(len, item_offset, item_length);
    if buffer.buffer.is_empty() && start > 0 {
        set_pre_context_utf16(&mut buffer.buffer, &units[..start]);
    }
    set_post_context_utf16(&mut buffer.buffer, &units[end..]);
    let item = &units[start..end];
    // One code unit per glyph is an upper bound. Reserving it avoids
    // decoding the same text once to count and again to append it.
    let capacity = buffer.buffer.len().saturating_add(item.len());
    if !buffer.buffer.reserve(capacity) {
        return;
    }
    let mut cluster = start;
    for decoded in char::decode_utf16(item.iter().copied()) {
        let (codepoint, width) = match decoded {
            Ok(character) => (character as u32, character.len_utf16()),
            Err(_) => (char::REPLACEMENT_CHARACTER as u32, 1),
        };
        buffer.buffer.push(codepoint, cluster as c_uint);
        cluster += width;
    }
}

/// Appends Latin-1 text. Offsets and clusters count bytes.
/// A negative `text_length` reads to a zero byte; a negative `item_length`
/// selects the rest of the text.
///
/// # Safety
///
/// `text` must point to `text_length` readable bytes, or be zero-terminated.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add_latin1(
    buffer: *mut hr_buffer_t,
    text: *const u8,
    text_length: c_int,
    item_offset: c_uint,
    item_length: c_int,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    if text.is_null() {
        return;
    }
    let bytes = if text_length < 0 {
        unsafe { core::ffi::CStr::from_ptr(text.cast()) }.to_bytes()
    } else {
        unsafe { core::slice::from_raw_parts(text, text_length as usize) }
    };
    let (start, end) = item_range(bytes.len(), item_offset, item_length);
    if buffer.buffer.is_empty() && start > 0 {
        let mut pre = [0; MAX_CONTEXT_CODEPOINTS];
        let len = start.min(pre.len());
        for (slot, &byte) in pre[..len].iter_mut().zip(bytes[..start].iter().rev()) {
            *slot = u32::from(byte);
        }
        buffer.buffer.set_pre_context_codepoints(&pre[..len]);
    }
    let mut post = [0; MAX_CONTEXT_CODEPOINTS];
    let len = (bytes.len() - end).min(post.len());
    for (slot, &byte) in post[..len].iter_mut().zip(&bytes[end..]) {
        *slot = u32::from(byte);
    }
    buffer.buffer.set_post_context_codepoints(&post[..len]);
    let capacity = buffer.buffer.len().saturating_add(end - start);
    if !buffer.buffer.reserve(capacity) {
        return;
    }
    for (index, &byte) in bytes[start..end].iter().enumerate() {
        buffer
            .buffer
            .push(u32::from(byte), (start + index) as c_uint);
    }
}

/// Appends UTF-8 text to a buffer.
///
/// Only `text[item_offset .. item_offset + item_length]` is added; the text
/// around it becomes shaping context. A negative `item_length` means "to the
/// end", and a negative `text_length` means `text` is NUL-terminated. Cluster
/// values are byte offsets into `text`.
///
/// # Safety
///
/// `text` must point to `text_length` readable bytes, or be NUL-terminated
/// when `text_length` is negative.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add_utf8(
    buffer: *mut hr_buffer_t,
    text: *const c_char,
    text_length: c_int,
    item_offset: c_uint,
    item_length: c_int,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    if text.is_null() {
        return;
    }
    let bytes = if text_length < 0 {
        unsafe { core::ffi::CStr::from_ptr(text) }.to_bytes()
    } else {
        unsafe { core::slice::from_raw_parts(text.cast::<u8>(), text_length as usize) }
    };
    let (start, end) = item_range(bytes.len(), item_offset, item_length);

    // What comes before the item is context only for a buffer that holds
    // nothing yet: a caller may set the context in one call and add the text
    // in the next, and text added after that has its own surroundings.
    if buffer.buffer.is_empty() && start > 0 {
        set_pre_context_utf8(&mut buffer.buffer, &bytes[..start]);
    }
    // What comes after is rebuilt every time, so that context left from an
    // earlier call cannot outlive the text it followed. An item running to
    // the end of the text has nothing after it, and says so.
    set_post_context_utf8(&mut buffer.buffer, &bytes[end..]);
    // Cluster values are offsets into the whole text, not into the item.
    //
    // Well-formed text is the overwhelmingly common case, and validating it
    // outright is faster than the lossy decoder, which walks the bytes once to
    // find the ill-formed sequences and then again to yield the characters.
    let item = &bytes[start..end];
    if let Ok(text) = core::str::from_utf8(item) {
        // Well-formed text, which is the whole point: this is the hottest
        // call in the API and it should not allocate.
        append_utf8(&mut buffer.buffer, text, start as c_uint);
    } else {
        // Ill-formed text goes one codepoint at a time, so that each bad
        // byte becomes its own replacement carrying its own offset, as
        // `hb_utf8_t::next` does. Decoding it lossily instead would fold a
        // run of bad bytes into a single replacement and number the clusters
        // against the replacement text rather than the caller's.
        let mut index = 0;
        while index < item.len() {
            let cluster = start + index;
            let codepoint = next_utf8(item, &mut index);
            buffer.buffer.push(codepoint, cluster as c_uint);
        }
    }
}

/// Appends UTF-32 codepoints to a buffer.
///
/// Follows the same `item_offset` / `item_length` convention as
/// [`hr_buffer_add_utf8`], with cluster values counted in codepoints.
///
/// # Safety
///
/// `text` must point to `text_length` readable codepoints, or be terminated
/// by a zero when `text_length` is negative.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add_utf32(
    buffer: *mut hr_buffer_t,
    text: *const u32,
    text_length: c_int,
    item_offset: c_uint,
    item_length: c_int,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    if text.is_null() {
        return;
    }
    let len = unsafe { utf32_len(text, text_length) };
    unsafe { add_utf32_items(buffer, text, len, item_offset, item_length, true) };
}

/// How many 32-bit items `text` holds, counting to the zero terminator when
/// the caller did not say.
///
/// # Safety
///
/// `text` must hold `text_length` readable entries, or be zero terminated.
unsafe fn utf32_len(text: *const hr_codepoint_t, text_length: c_int) -> usize {
    if text_length >= 0 {
        return text_length as usize;
    }
    let mut len = 0usize;
    // SAFETY: the caller guarantees a zero terminator.
    while unsafe { *text.add(len) } != 0 {
        len += 1;
    }
    len
}

/// Appends 32-bit items, replacing the ones that are not characters only when
/// asked to.
///
/// # Safety
///
/// `text` must point to `len` readable entries.
unsafe fn add_utf32_items(
    buffer: &mut hr_buffer_t,
    text: *const hr_codepoint_t,
    len: usize,
    item_offset: c_uint,
    item_length: c_int,
    validate: bool,
) {
    let items = unsafe { core::slice::from_raw_parts(text, len) };
    let (start, end) = item_range(len, item_offset, item_length);

    // Surrogates and values past the last plane are not characters. HarfBuzz
    // replaces them in `add_utf32` and passes them through in
    // `add_codepoints`, which promises no validation at all.
    let scalar = |codepoint: u32| match char::from_u32(codepoint) {
        Some(_) => codepoint,
        None if validate => char::REPLACEMENT_CHARACTER as u32,
        None => codepoint,
    };
    // As in `hr_buffer_add_utf8`: what precedes the item is context only for
    // a buffer holding nothing yet, and what follows it is rebuilt each time.
    // Only the nearest few items on either side are context, so that a caller
    // shaping a long text run by run does not pay for the whole text on each
    // call.
    if buffer.buffer.is_empty() && start > 0 {
        let mut pre = [0; MAX_CONTEXT_CODEPOINTS];
        let len = start.min(pre.len());
        for (slot, &codepoint) in pre[..len].iter_mut().zip(items[..start].iter().rev()) {
            *slot = scalar(codepoint);
        }
        buffer.buffer.set_pre_context_codepoints(&pre[..len]);
    }
    let mut post = [0; MAX_CONTEXT_CODEPOINTS];
    let len = (items.len() - end).min(post.len());
    for (slot, &codepoint) in post[..len].iter_mut().zip(&items[end..]) {
        *slot = scalar(codepoint);
    }
    buffer.buffer.set_post_context_codepoints(&post[..len]);
    for (index, &codepoint) in items[start..end].iter().enumerate() {
        buffer
            .buffer
            .push(scalar(codepoint), (start + index) as c_uint);
    }
}

/// Appends codepoints to a buffer, whatever they are.
///
/// As [`hr_buffer_add_utf32`], except that nothing is checked: surrogates and
/// values past the last plane reach the buffer as themselves rather than as
/// replacement characters. This is the non-validating counterpart HarfBuzz
/// provides for callers that have already checked, or that mean it.
///
/// # Safety
///
/// See [`hr_buffer_add_utf32`].
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_add_codepoints(
    buffer: *mut hr_buffer_t,
    text: *const hr_codepoint_t,
    text_length: c_int,
    item_offset: c_uint,
    item_length: c_int,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    if text.is_null() {
        return;
    }
    let len = unsafe { utf32_len(text, text_length) };
    unsafe { add_utf32_items(buffer, text, len, item_offset, item_length, false) };
}

/// Appends a range of one buffer's items to another.
///
/// # Safety
///
/// Both buffers must be `NULL` or live, and must not be the same buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_append(
    buffer: *mut hr_buffer_t,
    source: *const hr_buffer_t,
    start: c_uint,
    end: c_uint,
) {
    if core::ptr::eq(buffer.cast_const(), source) {
        return;
    }
    let Some(source) = (unsafe { source.as_ref() }) else {
        return;
    };
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    let source_len = source.buffer.glyph_infos().len();
    let start = (start as usize).min(source_len);
    let end = (end as usize).clamp(start, source_len);
    if start == end {
        return;
    }

    let orig_len = buffer.buffer.glyph_infos().len();
    // A destination with nothing in it takes the source's kind; one that
    // already holds glyphs is required to agree with the source already.
    if orig_len == 0 {
        buffer.buffer.set_content_type(source.buffer.content_type());
    }

    // Properties the destination has none of its own come from the source,
    // as `hr_segment_properties_overlay` does -- and stop where the two
    // describe the text differently, since the rest of what the source says
    // is then about other text.
    if buffer.buffer.direction() == Direction::Invalid {
        buffer.buffer.set_direction(source.buffer.direction());
    }
    if buffer.buffer.direction() == source.buffer.direction() {
        if buffer.buffer.script().is_none() {
            buffer.buffer.set_script(source.buffer.script());
        }
        if buffer.buffer.script() == source.buffer.script() && buffer.buffer.language().is_none() {
            buffer
                .buffer
                .set_language(source.buffer.language().cloned());
        }
    }

    if !buffer
        .buffer
        .push_glyph_infos(&source.buffer.glyph_infos()[start..end])
    {
        return;
    }

    // Positions travel with the glyphs they belong to. Appending to a
    // destination that has them but from a source that does not would
    // otherwise leave whatever the allocation happened to hold.
    let source_positions = source.buffer.glyph_positions();
    if !source_positions.is_empty() {
        buffer.buffer.glyph_positions_mut()[orig_len..]
            .copy_from_slice(&source_positions[start..end]);
    } else if !buffer.buffer.glyph_positions().is_empty() {
        buffer.buffer.glyph_positions_mut()[orig_len..].fill(GlyphPosition::default());
    }

    // Text carries its surroundings: what the copied range had around it in
    // the source, and then whatever the source itself had around that.
    if source.buffer.content_type() == Some(ContentType::Unicode) {
        let infos = source.buffer.glyph_infos();
        let pre = source.buffer.pre_context_codepoints();
        if orig_len == 0 && (start > 0 || !pre.is_empty()) {
            let mut context = [0; MAX_CONTEXT_CODEPOINTS];
            let mut len = 0;
            let mut at = start;
            // Pre-context runs backwards from the start of the range.
            while at > 0 && len < context.len() {
                at -= 1;
                context[len] = infos[at].glyph_id;
                len += 1;
            }
            for &codepoint in pre {
                if len == context.len() {
                    break;
                }
                context[len] = codepoint;
                len += 1;
            }
            buffer.buffer.set_pre_context_codepoints(&context[..len]);
        }

        let mut context = [0; MAX_CONTEXT_CODEPOINTS];
        let mut len = 0;
        for &info in &infos[end..] {
            if len == context.len() {
                break;
            }
            context[len] = info.glyph_id;
            len += 1;
        }
        for &codepoint in source.buffer.post_context_codepoints() {
            if len == context.len() {
                break;
            }
            context[len] = codepoint;
            len += 1;
        }
        buffer.buffer.set_post_context_codepoints(&context[..len]);
    }
}

/// Returns what a buffer currently holds.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_content_type(
    buffer: *mut hr_buffer_t,
) -> hr_buffer_content_type_t {
    content_type_from_rust(
        unsafe { object::or_empty(buffer.cast_const()) }
            .buffer
            .content_type(),
    )
}

/// Relabels what a buffer holds, without clearing it.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_content_type(
    buffer: *mut hr_buffer_t,
    content_type: hr_buffer_content_type_t,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer
            .buffer
            .set_content_type(content_type_to_rust(content_type));
    }
}

/// Sets a buffer's text direction.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_direction(
    buffer: *mut hr_buffer_t,
    direction: hr_direction_t,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.set_direction(direction_to_rust(direction));
    }
}

/// Returns a buffer's text direction.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_direction(buffer: *mut hr_buffer_t) -> hr_direction_t {
    direction_from_rust(
        unsafe { object::or_empty(buffer.cast_const()) }
            .buffer
            .direction(),
    )
}

/// Sets a buffer's script.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_script(buffer: *mut hr_buffer_t, script: hr_script_t) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    // `HR_SCRIPT_INVALID` is how a caller says "no script", not a script to
    // ignore: setting it has to clear whatever was there.
    buffer.buffer.set_script(script_to_rust(script));
}

/// Returns a buffer's script.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_script(buffer: *mut hr_buffer_t) -> hr_script_t {
    // A buffer that was never given a script reports none, rather than the
    // `Zzzz` that stands for a script known to be unknown.
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .script()
        .map_or(crate::common::HR_SCRIPT_INVALID, script_from_rust)
}

/// Sets a buffer's language.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `language` must be `NULL` or
/// a value returned by `hr_language_from_string`.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_language(buffer: *mut hr_buffer_t, language: hr_language_t) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    // As with the script, `NULL` clears rather than does nothing.
    buffer
        .buffer
        .set_language(unsafe { language_to_rust(language) });
}

/// Returns a buffer's language, or `NULL` if it has none.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_language(buffer: *mut hr_buffer_t) -> hr_language_t {
    language_from_rust(
        unsafe { object::or_empty(buffer.cast_const()) }
            .buffer
            .language(),
    )
}

/// Sets a buffer's flags.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_flags(buffer: *mut hr_buffer_t, flags: hr_buffer_flags_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer
            .buffer
            .set_flags(BufferFlags::from_bits_truncate(flags));
    }
}

/// Returns a buffer's flags.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_flags(buffer: *mut hr_buffer_t) -> hr_buffer_flags_t {
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .flags()
        .bits()
}

/// Sets a buffer's cluster level.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_cluster_level(
    buffer: *mut hr_buffer_t,
    cluster_level: hr_buffer_cluster_level_t,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer
            .buffer
            .set_cluster_level(cluster_level_to_rust(cluster_level));
    }
}

/// Returns a buffer's cluster level.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_cluster_level(
    buffer: *mut hr_buffer_t,
) -> hr_buffer_cluster_level_t {
    cluster_level_from_rust(
        unsafe { object::or_empty(buffer.cast_const()) }
            .buffer
            .cluster_level(),
    )
}

/// Returns the number of items in a buffer.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_length(buffer: *mut hr_buffer_t) -> c_uint {
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .len() as c_uint
}

/// Sets the number of items in a buffer, zero-filling any growth.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_length(
    buffer: *mut hr_buffer_t,
    length: c_uint,
) -> hr_bool_t {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return false.into();
    };
    if !buffer.buffer.set_length(length as usize) {
        return false.into();
    }
    // A buffer with nothing in it holds nothing of what came before it
    // either, and is no longer a buffer of anything in particular.
    if length == 0 {
        buffer.buffer.set_content_type(None);
        buffer.buffer.set_pre_context_codepoints(&[]);
    }
    // The text that followed does not follow this.
    buffer.buffer.set_post_context_codepoints(&[]);
    true.into()
}

/// Returns a buffer's items, writing their count to `length`.
///
/// Before shaping, each item's `codepoint` holds an input character. The
/// pointer stays valid until the buffer's contents change.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `length` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_glyph_infos(
    buffer: *mut hr_buffer_t,
    length: *mut c_uint,
) -> *mut hr_glyph_info_t {
    // The pointer handed out is writable, which the immortal empty buffer,
    // shared by every thread, must never be. It has no items to give anyway.
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        if let Some(out) = unsafe { length.as_mut() } {
            *out = 0;
        }
        return core::ptr::null_mut();
    };
    let infos = buffer.buffer.glyph_infos_mut();
    if let Some(out) = unsafe { length.as_mut() } {
        *out = infos.len() as c_uint;
    }
    // The layouts are asserted to match above.
    infos.as_mut_ptr().cast::<hr_glyph_info_t>()
}

/// Returns a buffer's glyph positions, writing their count to `length`.
///
/// Positions are allocated and zeroed on demand if the buffer has not been
/// shaped. The pointer stays valid until the buffer's contents change.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `length` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_glyph_positions(
    buffer: *mut hr_buffer_t,
    length: *mut c_uint,
) -> *mut hr_glyph_position_t {
    // Allocating positions on demand writes to the buffer, which the immortal
    // empty one, shared by every thread, must never see. It has none to give.
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        if let Some(out) = unsafe { length.as_mut() } {
            *out = 0;
        }
        return core::ptr::null_mut();
    };
    let positions = buffer.buffer.glyph_positions_mut();
    if let Some(out) = unsafe { length.as_mut() } {
        *out = positions.len() as c_uint;
    }
    positions.as_mut_ptr().cast::<hr_glyph_position_t>()
}

/// Returns whether a buffer has glyph positions, which is true after shaping.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_has_positions(buffer: *mut hr_buffer_t) -> hr_bool_t {
    (!unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .glyph_positions()
        .is_empty())
    .into()
}

/// Returns the flags shaping attached to one item.
///
/// # Safety
///
/// `info` must be `NULL` or point to a readable item.
#[no_mangle]
pub unsafe extern "C" fn hr_glyph_info_get_glyph_flags(
    info: *const hr_glyph_info_t,
) -> hr_glyph_flags_t {
    unsafe { info.as_ref() }.map_or(0, |info| info.mask & HR_GLYPH_FLAG_DEFINED)
}

/// Sets the glyph used in place of invisible characters.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_invisible_glyph(
    buffer: *mut hr_buffer_t,
    invisible: hr_codepoint_t,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        let glyph = (invisible != 0).then(|| harfrust::GlyphId::from(invisible));
        buffer.buffer.set_invisible_glyph(glyph);
    }
}

/// Returns the glyph used in place of invisible characters, or zero.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_invisible_glyph(buffer: *mut hr_buffer_t) -> hr_codepoint_t {
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .invisible_glyph()
        .map_or(0, harfrust::GlyphId::to_u32)
}

/// Sets the glyph used in place of variation selectors the font lacks.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_not_found_variation_selector_glyph(
    buffer: *mut hr_buffer_t,
    glyph: hr_codepoint_t,
) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer
            .buffer
            // The codepoint that is not one asks for nothing to be
            // substituted, rather than for that glyph.
            .set_not_found_variation_selector_glyph(
                (glyph != HR_CODEPOINT_INVALID).then_some(glyph),
            );
    }
}

/// Returns the glyph used in place of variation selectors the font lacks.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_not_found_variation_selector_glyph(
    buffer: *mut hr_buffer_t,
) -> hr_codepoint_t {
    unsafe { object::or_empty(buffer.cast_const()) }
        .buffer
        .not_found_variation_selector_glyph()
        // Nothing set means nothing is substituted, which HarfBuzz spells
        // with the codepoint that is not one.
        .unwrap_or(HR_CODEPOINT_INVALID)
}

/// Writes a buffer's direction, script and language into `props`.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `props` must be `NULL` or
/// writable.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_get_segment_properties(
    buffer: *mut hr_buffer_t,
    props: *mut crate::shape_plan::hr_segment_properties_t,
) {
    let Some(out) = (unsafe { props.as_mut() }) else {
        return;
    };
    let buffer = unsafe { object::or_empty(buffer.cast_const()) };
    *out = crate::shape_plan::hr_segment_properties_t {
        direction: direction_from_rust(buffer.buffer.direction()),
        script: buffer
            .buffer
            .script()
            .map_or(crate::common::HR_SCRIPT_INVALID, script_from_rust),
        language: language_from_rust(buffer.buffer.language()),
        reserved1: core::ptr::null_mut(),
        reserved2: core::ptr::null_mut(),
    };
}

/// Sets a buffer's direction, script and language at once.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer, and `props` must be `NULL` or
/// readable.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_set_segment_properties(
    buffer: *mut hr_buffer_t,
    props: *const crate::shape_plan::hr_segment_properties_t,
) {
    let (Some(buffer), Some(props)) = (unsafe { object::as_mutable(buffer) }, unsafe {
        props.as_ref()
    }) else {
        return;
    };
    // Every property is assigned, including the absent ones: this overwrites
    // what the buffer held rather than filling in around it, so that a reused
    // buffer cannot keep a script or language from its last life.
    let (direction, script, language) = props.to_rust();
    buffer.buffer.set_direction(direction);
    buffer.buffer.set_script(script);
    buffer.buffer.set_language(language);
}

/// Fills in whichever of direction, script and language are still unset, by
/// inspecting the buffer's contents.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_guess_segment_properties(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        guess_segment_properties(&mut buffer.buffer);
    }
}

/// Fills in the direction, script and language a buffer is missing.
///
/// The language is the one the process is running under, which HarfBuzz also
/// settles on here: harfrust leaves it alone, having no business reading the
/// environment, but the C API answers for HarfBuzz's behaviour.
pub(crate) fn guess_segment_properties(buffer: &mut Buffer) {
    buffer.guess_segment_properties();
    if buffer.language().is_none() {
        buffer.set_language(Some(crate::common::default_language()));
    }
}

/// Reverses a buffer's contents.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reverse(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.reverse();
    }
}

/// Reverses part of a buffer's contents.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reverse_range(
    buffer: *mut hr_buffer_t,
    start: c_uint,
    end: c_uint,
) {
    let Some(buffer) = (unsafe { object::as_mutable(buffer) }) else {
        return;
    };
    let len = buffer.buffer.len();
    let start = (start as usize).min(len);
    let end = (end as usize).clamp(start, len);
    buffer.buffer.reverse_range(start, end);
}

/// Reverses a buffer's contents, keeping each cluster's items in order.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reverse_clusters(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.reverse_clusters();
    }
}

/// Resets every item's cluster value to its index.
///
/// # Safety
///
/// `buffer` must be `NULL` or a live buffer.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_reset_clusters(buffer: *mut hr_buffer_t) {
    if let Some(buffer) = unsafe { object::as_mutable(buffer) } {
        buffer.buffer.reset_clusters();
    }
}

/// Returns the serialization format matching a name, such as `"text"`.
///
/// # Safety
///
/// See `hr_tag_from_string`.
#[no_mangle]
pub unsafe extern "C" fn hr_buffer_serialize_format_from_string(
    str_: *const c_char,
    len: c_int,
) -> hr_buffer_serialize_format_t {
    // The name as a tag, upper-cased by clearing the bit that separates the
    // cases. A name this library does not know still comes back as itself,
    // as it does in HarfBuzz, rather than collapsing to the invalid format:
    // it is the caller's value to hold and hand back.
    unsafe { crate::common::hr_tag_from_string(str_, len) & !0x2020_2020 }
}

/// Returns the name of a serialization format, or `NULL` if it is invalid.
#[no_mangle]
pub extern "C" fn hr_buffer_serialize_format_to_string(
    format: hr_buffer_serialize_format_t,
) -> *const c_char {
    let name: &[u8] = match format {
        HR_BUFFER_SERIALIZE_FORMAT_TEXT => b"text\0",
        HR_BUFFER_SERIALIZE_FORMAT_JSON => b"json\0",
        // Including the invalid format, which names nothing.
        _ => return core::ptr::null(),
    };
    name.as_ptr().cast::<c_char>()
}

/// Returns the serialization formats this library supports, as a
/// `NULL`-terminated array of names.
#[no_mangle]
pub extern "C" fn hr_buffer_serialize_list_formats() -> *const *const c_char {
    static FORMATS: CStrArray<2> = CStrArray([c"text".as_ptr(), core::ptr::null()]);
    FORMATS.0.as_ptr()
}

/// Writes a human-readable form of `buffer[start..end]` into `buf`.
///
/// Returns the number of items serialized, writing the number of bytes used to
/// `buf_consumed`. Only
/// [`HR_BUFFER_SERIALIZE_FORMAT_TEXT`] is
/// supported; any other format serializes nothing.
///
/// # Safety
///
/// `buffer` and `font` must be `NULL` or live, `buf` must point to `buf_size`
/// writable bytes, and `buf_consumed` must be `NULL` or writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn hr_buffer_serialize_glyphs(
    buffer: *mut hr_buffer_t,
    start: c_uint,
    end: c_uint,
    buf: *mut c_char,
    buf_size: c_uint,
    buf_consumed: *mut c_uint,
    font: *mut hr_font_t,
    format: hr_buffer_serialize_format_t,
    flags: hr_buffer_serialize_flags_t,
) -> c_uint {
    let write_consumed = |len: c_uint| {
        if let Some(out) = unsafe { buf_consumed.as_mut() } {
            *out = len;
        }
    };
    write_consumed(0);

    if format != HR_BUFFER_SERIALIZE_FORMAT_TEXT {
        return 0;
    }
    let buffer = unsafe { object::or_empty(buffer.cast_const()) };
    // Kept as passed as well: callbacks are handed the font they were
    // installed on.
    let font_ptr = font;
    let font = unsafe { object::or_empty(font.cast_const()) };

    let infos = buffer.buffer.glyph_infos();
    let start = (start as usize).min(infos.len());
    let end = (end as usize).clamp(start, infos.len());
    // Nothing to write, but the destination is still terminated.
    if start == end {
        unsafe { write_c_string("", buf, buf_size) };
        return 0;
    }

    let mut flags = SerializeFlags::from_bits_truncate((flags & 0xFF) as u8);
    let positions = buffer.buffer.glyph_positions();
    // A buffer that was never given positions has none to report, which is
    // not the same as having zero ones: HarfBuzz turns the request off rather
    // than printing `+0` for every glyph.
    if positions.is_empty() {
        flags |= SerializeFlags::NO_POSITIONS;
    }
    // Extents are worked out here rather than by the serializer, which reads
    // the font's tables and so would not see callbacks the caller installed.
    // HarfBuzz asks the font, and leaves the field out when nothing answers.
    let wants_extents = flags.contains(SerializeFlags::GLYPH_EXTENTS);
    flags &= !SerializeFlags::GLYPH_EXTENTS;

    // With advances suppressed, each item reports the pen position it sits
    // at, which counts from the start of the buffer and not from the start of
    // the range. Carried as an offset on every item, since the serializer
    // accumulates within whatever it is given.
    let mut pen = (0i32, 0i32);
    if flags.contains(SerializeFlags::NO_ADVANCES) && !positions.is_empty() {
        for position in &positions[..start] {
            pen.0 = pen.0.saturating_add(position.x_advance);
            pen.1 = pen.1.saturating_add(position.y_advance);
        }
    }

    // One item at a time, so that the boundaries are where the items are
    // rather than wherever a separator happens to appear in the text. An
    // item is written when the whole of it fits and the count reports what
    // was written, which is what lets a caller serialize in several passes.
    let capacity = buf_size as usize;
    let serializer_font = font.shaper().cloned();
    let mut out = String::new();
    let mut written = 0;
    let mut single = Buffer::new();
    for (at, info) in infos[start..end].iter().enumerate() {
        single.clear();
        single.push_glyph_infos(core::slice::from_ref(info));
        if !positions.is_empty() {
            let mut position = positions[start + at];
            // The serializer starts its own pen at zero for every item, so
            // the running total rides along in the offsets.
            position.x_offset = position.x_offset.saturating_add(pen.0);
            position.y_offset = position.y_offset.saturating_add(pen.1);
            single.glyph_positions_mut()[0] = position;
        } else {
            // Give the serializer the positions it reads even though the
            // flags above keep them out of the output.
            single.glyph_positions_mut();
        }

        // Rebuilt per item: the flags are consumed by each call.
        let item_flags = SerializeFlags::from_bits_truncate(flags.bits());
        let item = single.serialize(serializer_font.as_ref(), item_flags);
        // One item serializes as a list of one; the brackets belong to the
        // list, and which one opens this item depends on where in the buffer
        // it sits, so that pieces serialized separately concatenate.
        let item = item
            .strip_prefix('[')
            .and_then(|item| item.strip_suffix(']'))
            .unwrap_or(&item);

        let mut chunk = String::with_capacity(item.len() + 2);
        chunk.push(if start + at == 0 { '[' } else { '|' });
        chunk.push_str(item);
        // Last in an item, after everything the serializer writes.
        if wants_extents {
            if let Some(extents) = font.glyph_extents(font_ptr, info.glyph_id) {
                use core::fmt::Write;
                let _ = write!(
                    chunk,
                    "<{},{},{},{}>",
                    extents.x_bearing, extents.y_bearing, extents.width, extents.height
                );
            }
        }
        if start + at == end - 1 {
            chunk.push(']');
        }
        // Room for the item and the terminator, or the caller is told to
        // make more rather than handed the start of an item.
        if out.len() + chunk.len() >= capacity {
            break;
        }
        out.push_str(&chunk);
        written += 1;

        if flags.contains(SerializeFlags::NO_ADVANCES) && !positions.is_empty() {
            let position = positions[start + at];
            pen.0 = pen.0.saturating_add(position.x_advance);
            pen.1 = pen.1.saturating_add(position.y_advance);
        }
    }

    unsafe { write_c_string(&out, buf, buf_size) };
    write_consumed(out.len() as c_uint);
    written as c_uint
}

#[cfg(test)]
mod tests {
    use super::*;

    const REPLACEMENT: u32 = char::REPLACEMENT_CHARACTER as u32;

    #[test]
    fn decodes_bounded_utf8_context() {
        let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
        let text = "!a¢€𐍈z";

        let len = decode_pre_context_utf8(text.as_bytes(), &mut codepoints);
        assert_eq!(
            &codepoints[..len],
            &['z' as u32, '𐍈' as u32, '€' as u32, '¢' as u32, 'a' as u32]
        );

        let len = decode_post_context_utf8(text.as_bytes(), &mut codepoints);
        assert_eq!(
            &codepoints[..len],
            &['!' as u32, 'a' as u32, '¢' as u32, '€' as u32, '𐍈' as u32]
        );
    }

    #[test]
    fn replaces_each_ill_formed_context_byte() {
        let mut codepoints = [0; MAX_CONTEXT_CODEPOINTS];
        let text = [0xC0, 0x80, 0xE2, 0x82, b'a', 0xF0, 0x9F];

        let len = decode_pre_context_utf8(&text, &mut codepoints);
        assert_eq!(
            &codepoints[..len],
            &[
                REPLACEMENT,
                REPLACEMENT,
                'a' as u32,
                REPLACEMENT,
                REPLACEMENT,
            ]
        );

        let len = decode_post_context_utf8(&text, &mut codepoints);
        assert_eq!(
            &codepoints[..len],
            &[
                REPLACEMENT,
                REPLACEMENT,
                REPLACEMENT,
                REPLACEMENT,
                'a' as u32,
            ]
        );
    }

    #[test]
    fn decodes_utf16_context_in_codepoints() {
        let units = [b'a' as u16, 0xD83D, 0xDE00, b'b' as u16, 0xD800, 0xDC00];
        let mut buffer = Buffer::new();
        set_pre_context_utf16(&mut buffer, &units);
        assert_eq!(
            buffer.pre_context_codepoints(),
            &[0x10000, b'b' as u32, 0x1F600, b'a' as u32]
        );
        set_post_context_utf16(&mut buffer, &units);
        assert_eq!(
            buffer.post_context_codepoints(),
            &[b'a' as u32, 0x1F600, b'b' as u32, 0x10000]
        );

        let broken = [0xD800, b'x' as u16, 0xDC00];
        set_pre_context_utf16(&mut buffer, &broken);
        assert_eq!(
            buffer.pre_context_codepoints(),
            &[REPLACEMENT, b'x' as u32, REPLACEMENT]
        );
        set_post_context_utf16(&mut buffer, &broken);
        assert_eq!(
            buffer.post_context_codepoints(),
            &[REPLACEMENT, b'x' as u32, REPLACEMENT]
        );

        let long = [
            b'0' as u16,
            b'1' as u16,
            b'2' as u16,
            b'3' as u16,
            b'4' as u16,
            b'5' as u16,
            0xD83D,
            0xDE00,
        ];
        set_pre_context_utf16(&mut buffer, &long);
        assert_eq!(
            buffer.pre_context_codepoints(),
            &[0x1F600, b'5' as u32, b'4' as u32, b'3' as u32, b'2' as u32]
        );
    }

    #[test]
    fn keeps_bounded_utf32_context() {
        // Five items of context on either side of the item, nearest first,
        // with the ones that are not characters replaced only by `add_utf32`.
        let mut text: Vec<u32> = "abcdefgXhijklm".chars().map(u32::from).collect();
        text[2] = 0xD800;
        text[10] = 0x11_0000;
        let codepoints = |text: &str| text.chars().map(u32::from).collect::<Vec<_>>();
        unsafe {
            let buffer = hr_buffer_create();
            hr_buffer_add_utf32(buffer, text.as_ptr(), text.len() as c_int, 7, 1);
            let context = &(*buffer).buffer;
            assert_eq!(context.pre_context_codepoints(), codepoints("gfed\u{FFFD}"));
            assert_eq!(
                context.post_context_codepoints(),
                codepoints("hi\u{FFFD}kl")
            );

            hr_buffer_clear_contents(buffer);
            hr_buffer_add_codepoints(buffer, text.as_ptr(), text.len() as c_int, 7, 1);
            let context = &(*buffer).buffer;
            assert_eq!(context.pre_context_codepoints()[4], 0xD800);
            assert_eq!(context.post_context_codepoints()[2], 0x11_0000);

            // Less than five on a side, and an item running to the end.
            hr_buffer_clear_contents(buffer);
            hr_buffer_add_utf32(buffer, text.as_ptr(), text.len() as c_int, 2, -1);
            let context = &(*buffer).buffer;
            assert_eq!(context.pre_context_codepoints(), codepoints("ba"));
            assert_eq!(context.post_context_codepoints(), &[]);
            hr_buffer_destroy(buffer);
        }
    }
}
