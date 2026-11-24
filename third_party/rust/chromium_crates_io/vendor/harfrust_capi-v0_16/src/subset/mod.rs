//! Font subsetting through fontations' skera, using HarfRust C API handles.
//!
//! Enable the `subset` feature of `harfrust_capi`. The `hr_` prefix keeps
//! symbols separate from the C++ HarfBuzz subsetter. `hr-hb-subset.h` provides
//! HarfBuzz source aliases, not compatibility with HarfBuzz's opaque objects.

mod user_data;
use crate::{hr_blob_t, hr_face_t, hr_map_t, hr_set_t};

/// Font subsetting flags, matching HarfBuzz.
pub type hr_subset_flags_t = c_uint;

use core::{
    ffi::{c_int, c_uint, c_void},
    ptr,
};
use std::{
    panic::{catch_unwind, AssertUnwindSafe},
    sync::atomic::{AtomicUsize, Ordering},
};
use write_fonts::{
    read::{collections::IntSet, FontRef, TableProvider},
    types::{GlyphId, NameId, Tag},
    FontBuilder,
};

/// All supported subset flags are unset.
pub const HR_SUBSET_FLAGS_DEFAULT: hr_subset_flags_t = 0;
/// Remove TrueType hinting instructions.
pub const HR_SUBSET_FLAGS_NO_HINTING: hr_subset_flags_t = 0x1;
/// Keep original glyph IDs, leaving holes for removed glyphs.
pub const HR_SUBSET_FLAGS_RETAIN_GIDS: hr_subset_flags_t = 0x2;
/// Preserve the outline of glyph zero.
pub const HR_SUBSET_FLAGS_NOTDEF_OUTLINE: hr_subset_flags_t = 0x40;

/// Retain non-Unicode name records.
pub const HR_SUBSET_FLAGS_NAME_LEGACY: hr_subset_flags_t = 0x8;
/// Set the TrueType overlap flag on simple glyphs.
pub const HR_SUBSET_FLAGS_SET_OVERLAPS_FLAG: hr_subset_flags_t = 0x10;
/// Copy unrecognized tables unchanged.
pub const HR_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED: hr_subset_flags_t = 0x20;
/// Retain PostScript glyph names.
pub const HR_SUBSET_FLAGS_GLYPH_NAMES: hr_subset_flags_t = 0x80;
/// Preserve the OS/2 Unicode range bits.
pub const HR_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES: hr_subset_flags_t = 0x100;
/// Disable layout substitution glyph closure.
pub const HR_SUBSET_FLAGS_NO_LAYOUT_CLOSURE: hr_subset_flags_t = 0x200;
/// Disable mirrored Unicode closure.
pub const HR_SUBSET_FLAGS_NO_BIDI_CLOSURE: hr_subset_flags_t = 0x800;

const SUPPORTED_FLAGS: c_uint = HR_SUBSET_FLAGS_NO_HINTING
    | HR_SUBSET_FLAGS_RETAIN_GIDS
    | HR_SUBSET_FLAGS_NAME_LEGACY
    | HR_SUBSET_FLAGS_SET_OVERLAPS_FLAG
    | HR_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED
    | HR_SUBSET_FLAGS_NOTDEF_OUTLINE
    | HR_SUBSET_FLAGS_GLYPH_NAMES
    | HR_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES
    | HR_SUBSET_FLAGS_NO_LAYOUT_CLOSURE
    | HR_SUBSET_FLAGS_NO_BIDI_CLOSURE;

/// Identifies a configurable input set, with HarfBuzz's numeric values.
pub type hr_subset_sets_t = c_uint;
/// Glyph IDs to retain.
pub const HR_SUBSET_SETS_GLYPH_INDEX: hr_subset_sets_t = 0;
/// Unicode codepoints to retain.
pub const HR_SUBSET_SETS_UNICODE: hr_subset_sets_t = 1;
/// Tables to copy unchanged.
pub const HR_SUBSET_SETS_NO_SUBSET_TABLE_TAG: hr_subset_sets_t = 2;
/// Tables to omit from the output.
pub const HR_SUBSET_SETS_DROP_TABLE_TAG: hr_subset_sets_t = 3;
/// Name IDs to retain.
pub const HR_SUBSET_SETS_NAME_ID: hr_subset_sets_t = 4;
/// Name language IDs to retain.
pub const HR_SUBSET_SETS_NAME_LANG_ID: hr_subset_sets_t = 5;
/// Layout feature tags to retain.
pub const HR_SUBSET_SETS_LAYOUT_FEATURE_TAG: hr_subset_sets_t = 6;
/// Layout script tags to retain.
pub const HR_SUBSET_SETS_LAYOUT_SCRIPT_TAG: hr_subset_sets_t = 7;

const DEFAULT_NO_SUBSET_TABLES: [Tag; 5] = [
    Tag::new(b"gasp"),
    Tag::new(b"fpgm"),
    Tag::new(b"prep"),
    Tag::new(b"VDMX"),
    Tag::new(b"DSIG"),
];

/// Subsetting configuration, owning its glyph and Unicode sets.
///
/// Mutation requires exclusive access. Returned sets are borrowed; take a
/// set reference if it must outlive this input.
pub struct hr_subset_input_t {
    references: AtomicUsize,
    sets: [*mut hr_set_t; 8],
    flags: c_uint,
    user_data: user_data::UserData,
}

impl Drop for hr_subset_input_t {
    fn drop(&mut self) {
        self.user_data.release();
        unsafe {
            for set in self.sets {
                crate::hr_set_destroy(set);
            }
        }
    }
}

/// Creates a subset input with empty glyph and Unicode selections.
///
/// As in the shaping C API, Rust allocation failure aborts the process.
#[no_mangle]
pub extern "C" fn hr_subset_input_create_or_fail() -> *mut hr_subset_input_t {
    let sets = std::array::from_fn(|_| crate::hr_set_create());
    unsafe {
        for tag in DEFAULT_NO_SUBSET_TABLES {
            crate::hr_set_add(sets[2], u32::from_be_bytes(tag.to_be_bytes()));
        }
        for tag in skera::DEFAULT_DROP_TABLES {
            crate::hr_set_add(sets[3], u32::from_be_bytes(tag.to_be_bytes()));
        }
        crate::hr_set_add_range(sets[4], 0, 6);
        crate::hr_set_add(sets[5], 0x0409);
        for tag in skera::DEFAULT_LAYOUT_FEATURES {
            crate::hr_set_add(sets[6], u32::from_be_bytes(tag.to_be_bytes()));
        }
        crate::hr_set_invert(sets[7]);
    }
    Box::into_raw(Box::new(hr_subset_input_t {
        references: AtomicUsize::new(1),
        sets,
        flags: HR_SUBSET_FLAGS_DEFAULT,
        user_data: user_data::UserData::default(),
    }))
}

/// Takes another reference to a live input. `NULL` stays `NULL`.
///
/// # Safety
/// `input` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_reference(
    input: *mut hr_subset_input_t,
) -> *mut hr_subset_input_t {
    if let Some(input) = unsafe { input.as_ref() } {
        input.references.fetch_add(1, Ordering::Relaxed);
    }
    input
}

/// Releases an input reference and its owned sets. Accepts `NULL`.
///
/// # Safety
/// `input` must be `NULL` or an owned reference.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_destroy(input: *mut hr_subset_input_t) {
    let Some(state) = (unsafe { input.as_ref() }) else {
        return;
    };
    if state.references.fetch_sub(1, Ordering::Release) == 1 {
        std::sync::atomic::fence(Ordering::Acquire);
        // Callbacks may query the object. Run them while access is shared,
        // before reconstructing the Box and taking exclusive ownership.
        state.user_data.release();
        drop(unsafe { Box::from_raw(input) });
    }
}

/// Returns the borrowed set of glyph IDs to retain, or `NULL` for no input.
///
/// # Safety
/// `input` must be `NULL` or live; the returned set is borrowed from it.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_glyph_set(input: *mut hr_subset_input_t) -> *mut hr_set_t {
    unsafe { input.as_ref() }.map_or(ptr::null_mut(), |input| input.sets[0])
}

/// Returns the borrowed set of Unicode codepoints to retain.
///
/// # Safety
/// `input` must be `NULL` or live; the returned set is borrowed from it.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_unicode_set(
    input: *mut hr_subset_input_t,
) -> *mut hr_set_t {
    unsafe { input.as_ref() }.map_or(ptr::null_mut(), |input| input.sets[1])
}

/// Returns a borrowed configurable input set, or `NULL` for an invalid selector.
///
/// # Safety
/// `input` must be `NULL` or live; the returned set is borrowed from it.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_set(
    input: *mut hr_subset_input_t,
    set_type: hr_subset_sets_t,
) -> *mut hr_set_t {
    unsafe { input.as_ref() }
        .and_then(|input| input.sets.get(set_type as usize).copied())
        .unwrap_or(ptr::null_mut())
}

/// Configures all glyphs, Unicodes, names, and layout items to be retained.
/// The input can be tailored afterwards. Table passthrough and glyph names
/// are enabled; no tables are explicitly dropped.
///
/// # Safety
/// `input` must be `NULL` or live and exclusively accessible.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_keep_everything(input: *mut hr_subset_input_t) {
    let Some(input) = (unsafe { input.as_mut() }) else {
        return;
    };
    unsafe {
        for i in [0, 1, 4, 5, 6, 7] {
            crate::hr_set_clear(input.sets[i]);
            crate::hr_set_invert(input.sets[i]);
        }
        crate::hr_set_clear(input.sets[3]);
    }
    input.flags = HR_SUBSET_FLAGS_NOTDEF_OUTLINE
        | HR_SUBSET_FLAGS_GLYPH_NAMES
        | HR_SUBSET_FLAGS_NAME_LEGACY
        | HR_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES
        | HR_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED;
}

/// Sets the subset flags. Unsupported flag bits cause subsetting to fail.
///
/// # Safety
/// `input` must be `NULL` or live and exclusively accessible.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_set_flags(input: *mut hr_subset_input_t, flags: c_uint) {
    if let Some(input) = unsafe { input.as_mut() } {
        input.flags = flags;
    }
}

/// Returns the configured flags, or zero for `NULL`.
///
/// # Safety
/// `input` must be `NULL` or live and not concurrently mutated.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_get_flags(
    input: *const hr_subset_input_t,
) -> hr_subset_flags_t {
    unsafe { input.as_ref() }.map_or(0, |input| input.flags)
}

// A reference returned by the shaping API, released even if skera panics.
struct Blob(*mut hr_blob_t);
impl Drop for Blob {
    fn drop(&mut self) {
        unsafe { crate::hr_blob_destroy(self.0) };
    }
}

unsafe fn font_bytes(face: *mut hr_face_t) -> Option<Vec<u8>> {
    // Enumeration also handles callback-only fonts and TTC faces, regardless
    // of the face's separately writable index metadata.
    let mut count = 0;
    let total = unsafe { crate::hr_face_get_table_tags(face, 0, &raw mut count, ptr::null_mut()) };
    if total == 0 || total > u16::MAX as u32 {
        return None;
    }
    let mut tags = vec![0; total as usize];
    count = total;
    let enumerated =
        unsafe { crate::hr_face_get_table_tags(face, 0, &raw mut count, tags.as_mut_ptr()) };
    if enumerated != total || count != total {
        return None;
    }
    // Tables that do not overlap add up to at most the face's blob; charge
    // each one to that budget so a directory whose records alias the same
    // bytes cannot have them copied once per tag. A callback face supplies
    // its own tables and has no such bound.
    let mut budget = unsafe { crate::object::or_empty(face.cast_const()) }
        .blob_len()
        .unwrap_or(usize::MAX);
    let mut builder = FontBuilder::new();
    for tag in tags {
        let blob = Blob(unsafe { crate::hr_face_reference_table(face, tag) });
        let mut len = 0;
        let data = unsafe { crate::hr_blob_get_data(blob.0, &raw mut len) };
        if data.is_null() && len != 0 {
            return None;
        }
        budget = budget.checked_sub(len as usize)?;
        let data = if len == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(data.cast::<u8>(), len as usize) }
        };
        builder.add_raw(Tag::from_be_bytes(tag.to_be_bytes()), data.to_vec());
    }
    Some(builder.build())
}

unsafe fn ranges(set: *mut hr_set_t, max: u32) -> IntSet<u32> {
    let mut result = IntSet::empty();
    let (mut first, mut last) = (u32::MAX, u32::MAX);
    while unsafe { crate::hr_set_next_range(set, &raw mut first, &raw mut last) } != 0 {
        if first > max {
            break;
        }
        result.insert_range(first..=last.min(max));
        if last >= max {
            break;
        }
    }
    result
}

struct Set(*mut hr_set_t);
impl Drop for Set {
    fn drop(&mut self) {
        unsafe { crate::hr_set_destroy(self.0) };
    }
}

unsafe fn tags(set: *mut hr_set_t) -> IntSet<Tag> {
    // Preserve the inverted representation: Skera recognizes it when collecting
    // layout scripts. Enumerating a dense all-tags set would visit billions of tags.
    let inverted = unsafe { crate::hr_set_is_inverted(set) } != 0;
    let copy = if inverted {
        let copy = Set(unsafe { crate::hr_set_copy(set) });
        unsafe { crate::hr_set_invert(copy.0) };
        Some(copy)
    } else {
        None
    };
    let source = copy.as_ref().map_or(set, |copy| copy.0);
    let mut result = IntSet::empty();
    for range in unsafe { ranges(source, u32::MAX) }.iter_ranges() {
        result.insert_range(
            Tag::from_be_bytes(range.start().to_be_bytes())
                ..=Tag::from_be_bytes(range.end().to_be_bytes()),
        );
    }
    if inverted {
        result.invert();
        result.remove(Tag::from_be_bytes(u32::MAX.to_be_bytes()));
    }
    result
}

unsafe fn make_plan(
    face: *mut hr_face_t,
    input: &hr_subset_input_t,
) -> Option<(Vec<u8>, skera::Plan)> {
    if input.flags & !SUPPORTED_FLAGS != 0 {
        return None;
    }
    let count = unsafe { crate::hr_face_get_glyph_count(face) };
    if count == 0 {
        return None;
    }
    let bytes = unsafe { font_bytes(face) }?;
    let font = FontRef::new(&bytes).ok()?;
    // skera currently has no CFF/CFF2/VARC rewriting. Returning failure lets
    // callers keep the full font instead of embedding a font without outlines.
    if [b"CFF ", b"CFF2", b"VARC"]
        .iter()
        .any(|tag| font.data_for_tag(Tag::new(tag)).is_some())
    {
        return None;
    }
    let glyphs: IntSet<GlyphId> = unsafe { ranges(input.sets[0], count - 1) }
        .iter_ranges()
        .map(|range| GlyphId::from(*range.start())..=GlyphId::from(*range.end()))
        .fold(IntSet::empty(), |mut set, range| {
            set.insert_range(range);
            set
        });
    let unicodes = unsafe { ranges(input.sets[1], 0x10_FFFF) };
    let drop_tables = unsafe { tags(input.sets[3]) };
    let scripts = unsafe { tags(input.sets[7]) };
    let features = unsafe { tags(input.sets[6]) };
    let mut name_ids = IntSet::<NameId>::empty();
    for range in unsafe { ranges(input.sets[4], u16::MAX.into()) }.iter_ranges() {
        name_ids
            .insert_range(NameId::from(*range.start() as u16)..=NameId::from(*range.end() as u16));
    }
    let mut name_languages = IntSet::<u16>::empty();
    for range in unsafe { ranges(input.sets[5], u16::MAX.into()) }.iter_ranges() {
        name_languages.insert_range(*range.start() as u16..=*range.end() as u16);
    }
    let mut plan = skera::Plan::new(
        &glyphs,
        &unicodes,
        &font,
        skera::SubsetFlags::from(input.flags as u16),
        &drop_tables,
        &scripts,
        &features,
        &name_ids,
        &name_languages,
    );
    plan.set_no_subset_tables(&unsafe { tags(input.sets[2]) });
    Some((bytes, plan))
}

/// Subsets a face with skera, returning a new face with serialized SFNT data.
///
/// Returns `NULL` for invalid input, unsupported flags or outline formats,
/// missing table enumeration, or skera failure. The original face and input
/// are unchanged. Rust panics in the subsetter are contained at this boundary.
///
/// # Safety
/// `face` and `input` must be `NULL` or live. Neither the input nor its sets
/// may be modified during the call. Face callbacks must satisfy HarfRust's
/// threading contract.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_or_fail(
    face: *mut hr_face_t,
    input: *const hr_subset_input_t,
) -> *mut hr_face_t {
    let Some(input) = (unsafe { input.as_ref() }) else {
        return ptr::null_mut();
    };
    let Ok(Some(bytes)) = catch_unwind(AssertUnwindSafe(|| {
        let (bytes, plan) = unsafe { make_plan(face, input) }?;
        skera::subset_font(&FontRef::new(&bytes).ok()?, &plan).ok()
    })) else {
        return ptr::null_mut();
    };
    unsafe { face_from_bytes(&bytes) }
}

unsafe fn face_from_bytes(bytes: &[u8]) -> *mut hr_face_t {
    let Ok(len) = bytes.len().try_into() else {
        return ptr::null_mut();
    };
    let blob = Blob(unsafe {
        crate::hr_blob_create_or_fail(
            bytes.as_ptr().cast(),
            len,
            crate::HR_MEMORY_MODE_DUPLICATE,
            ptr::null_mut(),
            None,
        )
    });
    unsafe { crate::hr_face_create_or_fail(blob.0, 0) }
}

/// Copies a font's tables into an immutable, self-contained face for reuse.
///
/// This preserves the preprocessing API's ownership contract; it currently
/// does not add a Skera acceleration cache. It accepts any readable SFNT,
/// including outline formats that the subset operation cannot yet rewrite.
///
/// # Safety
/// `face` must be `NULL` or live, with callbacks satisfying the core API contract.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_preprocess(face: *mut hr_face_t) -> *mut hr_face_t {
    let Ok(Some(bytes)) = catch_unwind(AssertUnwindSafe(|| unsafe { font_bytes(face) })) else {
        return ptr::null_mut();
    };
    let result = unsafe { face_from_bytes(&bytes) };
    unsafe { crate::hr_face_make_immutable(result) };
    result
}

struct Map(*mut hr_map_t);
impl Map {
    fn new() -> Self {
        Self(crate::hr_map_create())
    }
}
impl Drop for Map {
    fn drop(&mut self) {
        unsafe { crate::hr_map_destroy(self.0) };
    }
}

/// An immutable subset plan owning its font bytes and borrowed output maps.
/// Creating a plan snapshots its input. The input and original face can be
/// released afterwards; execution may be repeated or run concurrently.
pub struct hr_subset_plan_t {
    references: AtomicUsize,
    bytes: Vec<u8>,
    plan: skera::Plan,
    maps: [Map; 3],
    user_data: user_data::UserData,
}

impl Drop for hr_subset_plan_t {
    fn drop(&mut self) {
        self.user_data.release();
    }
}

/// Creates a self-contained plan and its glyph mappings, or `NULL` on failure.
///
/// # Safety
/// `face` and `input` must be `NULL` or live. Input sets must not be mutated
/// during this call; face callbacks must satisfy the core API contract.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_create_or_fail(
    face: *mut hr_face_t,
    input: *const hr_subset_input_t,
) -> *mut hr_subset_plan_t {
    let Some(input) = (unsafe { input.as_ref() }) else {
        return ptr::null_mut();
    };
    let Ok(Some((bytes, plan))) =
        catch_unwind(AssertUnwindSafe(|| unsafe { make_plan(face, input) }))
    else {
        return ptr::null_mut();
    };
    let maps = std::array::from_fn(|_| Map::new());
    for (old, new) in plan.old_to_new_glyph_mapping() {
        unsafe {
            crate::hr_map_set(maps[0].0, old.to_u32(), new.to_u32());
            crate::hr_map_set(maps[1].0, new.to_u32(), old.to_u32());
        }
    }
    for (unicode, old) in plan.unicode_to_old_glyph_mapping() {
        unsafe { crate::hr_map_set(maps[2].0, unicode, old.to_u32()) };
    }
    Box::into_raw(Box::new(hr_subset_plan_t {
        references: AtomicUsize::new(1),
        bytes,
        plan,
        maps,
        user_data: user_data::UserData::default(),
    }))
}

/// Takes a reference to a subset plan. `NULL` stays `NULL`.
///
/// # Safety
/// `plan` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_reference(
    plan: *mut hr_subset_plan_t,
) -> *mut hr_subset_plan_t {
    if let Some(plan) = unsafe { plan.as_ref() } {
        plan.references.fetch_add(1, Ordering::Relaxed);
    }
    plan
}

/// Releases an owned plan reference. Accepts `NULL`.
///
/// # Safety
/// `plan` must be `NULL` or an owned reference.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_destroy(plan: *mut hr_subset_plan_t) {
    let Some(state) = (unsafe { plan.as_ref() }) else {
        return;
    };
    if state.references.fetch_sub(1, Ordering::Release) == 1 {
        std::sync::atomic::fence(Ordering::Acquire);
        // Callbacks may query the object. Run them while access is shared,
        // before reconstructing the Box and taking exclusive ownership.
        state.user_data.release();
        drop(unsafe { Box::from_raw(plan) });
    }
}

/// Executes a subset plan, returning an independent face or `NULL` on failure.
///
/// # Safety
/// `plan` must be `NULL` or live; its borrowed maps must not be modified.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_execute_or_fail(
    plan: *mut hr_subset_plan_t,
) -> *mut hr_face_t {
    let Some(plan) = (unsafe { plan.as_ref() }) else {
        return ptr::null_mut();
    };
    let Ok(Some(bytes)) = catch_unwind(AssertUnwindSafe(|| {
        skera::subset_font(&FontRef::new(&plan.bytes).ok()?, &plan.plan).ok()
    })) else {
        return ptr::null_mut();
    };
    unsafe { face_from_bytes(&bytes) }
}

/// Returns the borrowed original-to-subset map, including closure glyphs.
/// Take a core map reference if it must outlive the plan.
///
/// # Safety
/// `plan` must be `NULL` or live. The returned map is read-only.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_old_to_new_glyph_mapping(
    plan: *const hr_subset_plan_t,
) -> *mut hr_map_t {
    unsafe { plan.as_ref() }.map_or(ptr::null_mut(), |plan| plan.maps[0].0)
}

/// Returns the borrowed subset-to-original map, including closure glyphs.
///
/// # Safety
/// `plan` must be `NULL` or live. The returned map is read-only.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_new_to_old_glyph_mapping(
    plan: *const hr_subset_plan_t,
) -> *mut hr_map_t {
    unsafe { plan.as_ref() }.map_or(ptr::null_mut(), |plan| plan.maps[1].0)
}

/// Returns the borrowed Unicode-to-original-glyph map.
///
/// # Safety
/// `plan` must be `NULL` or live. The returned map is read-only.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_unicode_to_old_glyph_mapping(
    plan: *const hr_subset_plan_t,
) -> *mut hr_map_t {
    unsafe { plan.as_ref() }.map_or(ptr::null_mut(), |plan| plan.maps[2].0)
}

/// Attaches owned metadata to a subset input; rejected data stays caller-owned.
/// Replacing/removing metadata invokes its destructor outside the object lock.
///
/// # Safety
/// `input` must be `NULL` or live. The key must outlive the object, and the
/// supplied data/destructor must be safe to release from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_set_user_data(
    input: *mut hr_subset_input_t,
    key: *const crate::hr_user_data_key_t,
    data: *mut c_void,
    destroy: crate::hr_destroy_func_t,
    replace: c_int,
) -> c_int {
    unsafe { input.as_ref() }
        .is_some_and(|input| input.user_data.set(key, data, destroy, replace != 0))
        .into()
}

/// Retrieves attached metadata, or `NULL` when absent.
///
/// # Safety
/// `input` must be `NULL` or live. Concurrent replacement may invalidate the returned pointer.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_input_get_user_data(
    input: *const hr_subset_input_t,
    key: *const crate::hr_user_data_key_t,
) -> *mut c_void {
    unsafe { input.as_ref() }.map_or(ptr::null_mut(), |input| input.user_data.get(key))
}

/// Attaches owned metadata to a subset plan; rejected data stays caller-owned.
/// Replacing/removing metadata invokes its destructor outside the object lock.
///
/// # Safety
/// `plan` must be `NULL` or live. The key must outlive the object, and the
/// supplied data/destructor must be safe to release from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_set_user_data(
    plan: *mut hr_subset_plan_t,
    key: *const crate::hr_user_data_key_t,
    data: *mut c_void,
    destroy: crate::hr_destroy_func_t,
    replace: c_int,
) -> c_int {
    unsafe { plan.as_ref() }
        .is_some_and(|plan| plan.user_data.set(key, data, destroy, replace != 0))
        .into()
}

/// Retrieves attached metadata, or `NULL` when absent.
///
/// # Safety
/// `plan` must be `NULL` or live. Concurrent replacement may invalidate the returned pointer.
#[no_mangle]
pub unsafe extern "C" fn hr_subset_plan_get_user_data(
    plan: *const hr_subset_plan_t,
    key: *const crate::hr_user_data_key_t,
) -> *mut c_void {
    unsafe { plan.as_ref() }.map_or(ptr::null_mut(), |plan| plan.user_data.get(key))
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::Cell;

    #[derive(Clone, Copy)]
    enum Owner {
        Input(*mut hr_subset_input_t),
        Plan(*mut hr_subset_plan_t),
    }

    struct Destruction {
        owner: Owner,
        key: crate::hr_user_data_key_t,
        calls: Cell<u32>,
        resource_released: Cell<bool>,
        observed_released: Cell<bool>,
        observed_empty: Cell<bool>,
        accepted: Cell<c_int>,
    }

    unsafe extern "C" fn release_resource(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state.resource_released.set(true);
    }

    unsafe extern "C" fn release_metadata(data: *mut c_void) {
        let state = unsafe { &*data.cast::<Destruction>() };
        state.calls.set(state.calls.get() + 1);
        state.observed_released.set(state.resource_released.get());
        // Replacing this key during Vec::drop used to drop the same item a
        // second time. Keep the reproducer bounded on that implementation.
        if state.calls.get() != 1 {
            return;
        }
        let (current, accepted) = unsafe {
            match state.owner {
                Owner::Input(input) => (
                    hr_subset_input_get_user_data(input, &raw const state.key),
                    hr_subset_input_set_user_data(input, &raw const state.key, data, None, 1),
                ),
                Owner::Plan(plan) => (
                    hr_subset_plan_get_user_data(plan, &raw const state.key),
                    hr_subset_plan_set_user_data(plan, &raw const state.key, data, None, 1),
                ),
            }
        };
        state.observed_empty.set(current.is_null());
        state.accepted.set(accepted);
    }

    fn check_destruction(owner: Owner) {
        let mut state = Destruction {
            owner,
            key: crate::hr_user_data_key_t { unused: 0 },
            calls: Cell::new(0),
            resource_released: Cell::new(false),
            observed_released: Cell::new(true),
            observed_empty: Cell::new(false),
            accepted: Cell::new(-1),
        };
        let data = (&raw mut state).cast();
        unsafe {
            match owner {
                Owner::Input(input) => {
                    let set = hr_subset_input_glyph_set(input);
                    assert_eq!(
                        crate::hr_set_set_user_data(
                            set,
                            &raw const state.key,
                            data,
                            Some(release_resource),
                            1
                        ),
                        1
                    );
                    assert_eq!(
                        hr_subset_input_set_user_data(
                            input,
                            &raw const state.key,
                            data,
                            Some(release_metadata),
                            1
                        ),
                        1
                    );
                    hr_subset_input_destroy(input);
                }
                Owner::Plan(plan) => {
                    let map = hr_subset_plan_old_to_new_glyph_mapping(plan);
                    assert_eq!(
                        crate::hr_map_set_user_data(
                            map,
                            &raw const state.key,
                            data,
                            Some(release_resource),
                            1
                        ),
                        1
                    );
                    assert_eq!(
                        hr_subset_plan_set_user_data(
                            plan,
                            &raw const state.key,
                            data,
                            Some(release_metadata),
                            1
                        ),
                        1
                    );
                    hr_subset_plan_destroy(plan);
                }
            }
        }
        assert_eq!(state.calls.get(), 1);
        assert!(!state.observed_released.get());
        assert!(state.resource_released.get());
        assert!(state.observed_empty.get());
        assert_eq!(state.accepted.get(), 0);
    }

    #[test]
    fn input_metadata_is_detached_before_callbacks_and_sets_outlive_it() {
        check_destruction(Owner::Input(hr_subset_input_create_or_fail()));
    }

    #[test]
    fn plan_metadata_is_detached_before_callbacks_and_maps_outlive_it() {
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
            let input = hr_subset_input_create_or_fail();
            let plan = hr_subset_plan_create_or_fail(face, input);
            assert!(!plan.is_null());
            hr_subset_input_destroy(input);
            crate::hr_face_destroy(face);
            crate::hr_blob_destroy(blob);
            check_destruction(Owner::Plan(plan));
        }
    }
}
