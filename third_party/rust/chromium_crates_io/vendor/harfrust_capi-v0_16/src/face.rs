//! Fonts as a set of tables, mirroring HarfBuzz's `hb-face.h`.

use core::ffi::{c_uint, c_void};
use core::sync::atomic::{AtomicPtr, Ordering};
use std::sync::{Arc, OnceLock};

use harfrust::font::{Blob, Font, TableFunction};
use harfrust::Tag;
use read_fonts::{
    model::Format,
    tables::cmap::{CmapIterLimits, CmapSubtable},
    TableProvider,
};

use crate::blob::hr_blob_t;
use crate::common::{hr_bool_t, hr_tag_t, tag_from_rust, tag_to_rust};
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};
use crate::plan::PlanCache;
use crate::set::hr_set_t;

/// Callback returning the data for one table of a face.
///
/// The callback owns the reference it returns; this library takes it over and
/// releases it. Return `NULL` for a table the face does not have.
pub type hr_reference_table_func_t = Option<
    unsafe extern "C" fn(
        face: *mut hr_face_t,
        tag: hr_tag_t,
        user_data: *mut c_void,
    ) -> *mut hr_blob_t,
>;

/// Lists a face's table tags. Returns the total number of available tags.
///
/// `table_count` gives the output capacity on entry and the number written
/// on return. A null output array asks only for the total.
pub type hr_get_table_tags_func_t = Option<
    unsafe extern "C" fn(
        face: *const hr_face_t,
        start_offset: c_uint,
        table_count: *mut c_uint,
        table_tags: *mut hr_tag_t,
        user_data: *mut c_void,
    ) -> c_uint,
>;

struct TableTagsFunc {
    func: hr_get_table_tags_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
}

impl Drop for TableTagsFunc {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            unsafe { destroy(self.user_data) };
        }
    }
}

/// A table callback together with the data it was registered with.
struct TableFunc {
    func: hr_reference_table_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
    /// Filled in once the face exists, so the callback can be handed the face
    /// it is being asked about. Tables are only ever fetched lazily, well
    /// after construction, so this is never read while still null in practice.
    face: AtomicPtr<hr_face_t>,
}

// SAFETY: `hr_face_create_for_tables` documents that the callback and its user
// data must be safe to use from any thread. `read-fonts` requires this because
// it may load a table from whichever thread first touches it.
unsafe impl Send for TableFunc {}
unsafe impl Sync for TableFunc {}

impl TableFunc {
    fn call(&self, tag: Tag) -> Option<Blob> {
        let func = self.func?;
        let face = self.face.load(Ordering::Acquire);
        // SAFETY: the caller registered this callback and promised it is safe
        // to invoke with the user data it supplied.
        let blob = unsafe { func(face, tag_from_rust(tag), self.user_data) };
        if blob.is_null() {
            return None;
        }
        // Keep the reference the callback handed us for as long as the bytes
        // are in use, not only the storage under it: the callback may have
        // tied the bytes to the blob through its user data, which is released
        // with the blob. HarfBuzz keeps the callback's blob the same way.
        // SAFETY: the callback returned a live blob and gave up its reference.
        let data = unsafe { hr_blob_t::into_data(blob) };
        if data.as_ref().is_empty() {
            return None;
        }
        Some(data)
    }
}

impl Drop for TableFunc {
    fn drop(&mut self) {
        if let Some(destroy) = self.destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(self.user_data) };
        }
    }
}

/// How a face gets at its table data.
enum FaceSource {
    /// No data at all; only the immortal empty face uses this.
    Empty,
    /// A blob, over which this face owns a reference.
    Blob {
        blob: *mut hr_blob_t,
        /// The selected font is independent of the writable index metadata.
        index: c_uint,
        /// The blob's table directory, read on first use; `None` if the blob
        /// holds no SFNT font at `index`.
        directory: OnceLock<Option<BlobDirectory>>,
    },
    /// A caller-supplied table callback.
    Function(Arc<TableFunc>),
}

impl FaceSource {
    /// The blob and table directory of a blob-backed face.
    fn blob_directory(&self) -> Option<(&hr_blob_t, &BlobDirectory)> {
        let FaceSource::Blob {
            blob,
            index,
            directory,
        } = self
        else {
            return None;
        };
        let blob = unsafe { blob.as_ref() }?;
        let directory = directory
            .get_or_init(|| BlobDirectory::new(blob.bytes(), *index))
            .as_ref()?;
        Some((blob, directory))
    }
}

/// The table directory of a blob-backed face, kept so that neither looking up
/// a table nor listing the tags has to walk every record again. A directory
/// can hold 65535 records, so reading it per lookup made fetching every table
/// of a face quadratic in the record count.
struct BlobDirectory {
    /// The tag of every record, in directory order.
    tags: Box<[Tag]>,
    /// The tag, offset and length of every table, sorted by tag. Of several
    /// records with one tag only the first is kept, which is what the linear
    /// scan `read-fonts` falls back to for an unsorted directory finds.
    by_tag: Box<[(Tag, u32, u32)]>,
}

impl BlobDirectory {
    fn new(bytes: &[u8], index: c_uint) -> Option<Self> {
        let font = read_fonts::FontRef::from_index(bytes, index).ok()?;
        let records = font.table_directory().table_records();
        let mut by_tag: Vec<_> = records
            .iter()
            .map(|record| (record.tag(), record.offset(), record.length()))
            .collect();
        // The sort is stable, so the first record of each tag survives.
        by_tag.sort_by_key(|&(tag, ..)| tag);
        by_tag.dedup_by_key(|&mut (tag, ..)| tag);
        Some(Self {
            tags: records.iter().map(|record| record.tag()).collect(),
            by_tag: by_tag.into_boxed_slice(),
        })
    }

    /// The byte range of the table with `tag`, which may lie outside the blob.
    fn range(&self, tag: Tag) -> Option<(usize, usize)> {
        let at = self
            .by_tag
            .binary_search_by_key(&tag, |&(tag, ..)| tag)
            .ok()?;
        let (_, offset, length) = self.by_tag[at];
        // A zero offset means no table, as in `FontRef::table_data`.
        if offset == 0 {
            return None;
        }
        let start = offset as usize;
        Some((start, start.checked_add(length as usize)?))
    }
}

impl Drop for FaceSource {
    fn drop(&mut self) {
        if let FaceSource::Blob { blob, .. } = *self {
            // SAFETY: the face owns this reference.
            unsafe { object::destroy(blob) };
        }
    }
}

/// A font face: the tables of one font, before any size or variation settings.
pub struct hr_face_t {
    header: ObjectHeader,
    /// `None` only for the immortal empty face.
    pub(crate) font: Option<Font>,
    index: c_uint,
    source: FaceSource,
    table_tags: Option<TableTagsFunc>,
    /// Shape plans built over this face, reused across `hr_shape` calls the way
    /// HarfBuzz's cached shape plans are.
    pub(crate) plans: PlanCache,
    /// Whether the `feat` table's setting arrays are in proportion to its size.
    pub(crate) feat_sane: OnceLock<bool>,
    /// The SFNT assembled for Skrifa and its outlines, built on first use by
    /// `rendering`. Image blobs handed to paint callbacks share the SFNT.
    #[cfg(feature = "draw")]
    pub(crate) render_data: OnceLock<Option<crate::rendering::RenderData>>,
}

impl hr_face_t {
    pub(crate) fn font(&self) -> Option<&Font> {
        self.font.as_ref()
    }

    pub(crate) fn upem(&self) -> c_uint {
        self.font
            .as_ref()
            .and_then(|font| font.tables().head().ok())
            .map_or(1000, |head| c_uint::from(head.units_per_em()))
    }

    /// The size of the blob this face reads its tables from, or `None` for a
    /// face that gets them from a callback.
    ///
    /// Tables that do not overlap cannot add up to more than this, so it
    /// bounds what assembling a copy of the face's tables may cost.
    #[cfg(any(feature = "draw", feature = "subset"))]
    pub(crate) fn blob_len(&self) -> Option<usize> {
        match &self.source {
            FaceSource::Empty => Some(0),
            // SAFETY: the face owns a reference to its blob.
            FaceSource::Blob { blob, .. } => {
                Some(unsafe { blob.as_ref() }.map_or(0, |blob| blob.bytes().len()))
            }
            FaceSource::Function(_) => None,
        }
    }

    /// The glyph count from `MAXP` if the face has one, else from `maxp`.
    pub(crate) fn glyph_count(&self) -> c_uint {
        self.font
            .as_ref()
            .and_then(|font| font.tables().selected_maxp().ok())
            .map_or(0, |maxp| maxp.num_glyphs())
    }
}

static EMPTY_FACE: OnceLock<Empty<hr_face_t>> = OnceLock::new();

impl Object for hr_face_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_FACE
            .get_or_init(|| {
                Empty::new(hr_face_t {
                    header: ObjectHeader::immortal(),
                    font: None,
                    index: 0,
                    source: FaceSource::Empty,
                    table_tags: None,
                    plans: PlanCache::default(),
                    feat_sane: OnceLock::new(),
                    #[cfg(feature = "draw")]
                    render_data: OnceLock::new(),
                })
            })
            .get()
    }
}

/// Returns the number of SFNT faces in a font blob, or zero otherwise.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_face_count(blob: *mut hr_blob_t) -> c_uint {
    let Some(blob) = (unsafe { blob.as_ref() }) else {
        return 0;
    };
    match Format::new(blob.bytes()) {
        Some(Format::Sfnt(count)) => count,
        _ => 0,
    }
}

/// Creates a face over the font at `index` within `blob`.
///
/// The face takes its own reference to the blob. Never returns `NULL`; a blob
/// that does not parse yields the empty face.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_face_create(blob: *mut hr_blob_t, index: c_uint) -> *mut hr_face_t {
    let face = unsafe { hr_face_create_or_fail(blob, index) };
    if face.is_null() {
        return hr_face_t::empty();
    }
    face
}

/// Creates a face over a blob, returning `NULL` if it does not parse.
///
/// # Safety
///
/// `blob` must be `NULL` or a live blob.
#[no_mangle]
pub unsafe extern "C" fn hr_face_create_or_fail(
    blob: *mut hr_blob_t,
    index: c_uint,
) -> *mut hr_face_t {
    let Some(blob_ref) = (unsafe { blob.as_ref() }) else {
        return core::ptr::null_mut();
    };
    let Some(font) = Font::new(blob_ref.blob.clone(), index) else {
        return core::ptr::null_mut();
    };
    // Keep the blob alive for as long as the face.
    let owned = unsafe { object::reference(blob) };
    object::create(hr_face_t {
        header: ObjectHeader::new(),
        font: Some(font),
        index,
        source: FaceSource::Blob {
            blob: owned,
            index,
            directory: OnceLock::new(),
        },
        table_tags: None,
        plans: PlanCache::default(),
        feat_sane: OnceLock::new(),
        #[cfg(feature = "draw")]
        render_data: OnceLock::new(),
    })
}

/// Creates a face whose tables are supplied by a callback.
///
/// The callback is invoked lazily, once per table, and may be called from any
/// thread; it and its user data must be safe to use concurrently. `destroy` is
/// called with `user_data` when the face is freed.
///
/// Never returns `NULL`.
///
/// # Safety
///
/// `reference_table_func` must be safe to call with `user_data` from any
/// thread, and must return either `NULL` or a blob reference it gives up
/// ownership of.
#[no_mangle]
pub unsafe extern "C" fn hr_face_create_for_tables(
    reference_table_func: hr_reference_table_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) -> *mut hr_face_t {
    // Nothing to call is nothing to build a face from, and the data was
    // handed over to be released either way.
    if reference_table_func.is_none() {
        if let Some(destroy) = destroy {
            // SAFETY: `destroy` was supplied alongside `user_data`.
            unsafe { destroy(user_data) };
        }
        return hr_face_t::empty();
    }
    let state = Arc::new(TableFunc {
        func: reference_table_func,
        user_data,
        destroy,
        face: AtomicPtr::new(core::ptr::null_mut()),
    });
    let for_closure = Arc::clone(&state);
    let table_fn: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> =
        Arc::new(move |tag| for_closure.call(tag));
    // A table-function font is built lazily and never fails here.
    let Some(font) = Font::new(TableFunction::new(table_fn), 0) else {
        return hr_face_t::empty();
    };
    let face = object::create(hr_face_t {
        header: ObjectHeader::new(),
        font: Some(font),
        index: 0,
        source: FaceSource::Function(Arc::clone(&state)),
        table_tags: None,
        plans: PlanCache::default(),
        feat_sane: OnceLock::new(),
        #[cfg(feature = "draw")]
        render_data: OnceLock::new(),
    });
    // Now that the face exists, let the callback see it. This is a plain
    // pointer, not a reference, so it does not keep the face alive.
    state.face.store(face, Ordering::Release);
    face
}

/// Returns the immortal empty face.
#[no_mangle]
pub extern "C" fn hr_face_get_empty() -> *mut hr_face_t {
    hr_face_t::empty()
}

/// Increments a face's reference count.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_reference(face: *mut hr_face_t) -> *mut hr_face_t {
    unsafe { object::reference(face) }
}

/// Decrements a face's reference count, freeing it at zero.
///
/// # Safety
///
/// `face` must be `NULL` or a live face, and the caller must own the
/// reference being released.
#[no_mangle]
pub unsafe extern "C" fn hr_face_destroy(face: *mut hr_face_t) {
    unsafe { object::destroy(face) };
}

/// Attaches user data to a face.
///
/// # Safety
///
/// `face` must be `NULL` or a live face, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_face_set_user_data(
    face: *mut hr_face_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(face, key, data, destroy, replace != 0) }.into()
}

/// Retrieves user data previously attached to a face.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_get_user_data(
    face: *mut hr_face_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(face, key) }
}

/// Marks a face immutable.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_make_immutable(face: *mut hr_face_t) {
    unsafe { object::make_immutable(face) };
}

/// Returns whether a face has been marked immutable.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_is_immutable(face: *mut hr_face_t) -> hr_bool_t {
    unsafe { object::is_immutable(face.cast_const()) }.into()
}

/// Returns the index this face was created with, within its collection.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_get_index(face: *mut hr_face_t) -> c_uint {
    unsafe { object::or_empty(face.cast_const()) }.index
}

/// Assigns the index metadata returned by [`hr_face_get_index`].
///
/// This does not change which font's tables the face reads. An immutable
/// face ignores the assignment.
///
/// # Safety
/// `face` must be `NULL` or live, and must not be accessed concurrently.
#[no_mangle]
pub unsafe extern "C" fn hr_face_set_index(face: *mut hr_face_t, index: c_uint) {
    if let Some(face) = unsafe { object::as_mutable(face) } {
        face.index = index;
    }
}

/// Installs the table enumeration callback, taking ownership of `user_data`.
///
/// Replacing a callback releases its data. Immutable faces reject the callback
/// and release the supplied data immediately.
///
/// # Safety
/// `face` must be `NULL` or live and must not be accessed concurrently.
/// The callback and its data must be safe to invoke from any thread.
#[no_mangle]
pub unsafe extern "C" fn hr_face_set_get_table_tags_func(
    face: *mut hr_face_t,
    func: hr_get_table_tags_func_t,
    user_data: *mut c_void,
    destroy: hr_destroy_func_t,
) {
    let callback = TableTagsFunc {
        func,
        user_data,
        destroy,
    };
    if let Some(face) = unsafe { object::as_mutable(face) } {
        let old = face.table_tags.replace(callback);
        drop(old);
    }
}

/// Lists the table tags of a face, beginning at `start_offset`.
///
/// Returns the total number of tags. A null output array asks only for the
/// total, without changing `table_count`. Callback-created faces require a
/// callback installed with [`hr_face_set_get_table_tags_func`].
///
/// # Safety
/// `face` must be `NULL` or live. Non-null outputs must be writable for their
/// advertised capacity.
#[no_mangle]
pub unsafe extern "C" fn hr_face_get_table_tags(
    face: *const hr_face_t,
    start_offset: c_uint,
    table_count: *mut c_uint,
    table_tags: *mut hr_tag_t,
) -> c_uint {
    let state = unsafe { object::or_empty(face) };
    if let Some(callback) = &state.table_tags {
        if let Some(func) = callback.func {
            return unsafe {
                func(
                    face,
                    start_offset,
                    table_count,
                    table_tags,
                    callback.user_data,
                )
            };
        }
    } else if let Some((_, directory)) = state.source.blob_directory() {
        let tags = &directory.tags;
        if !table_tags.is_null() {
            if let Some(count) = unsafe { table_count.as_mut() } {
                let start = (start_offset as usize).min(tags.len());
                let len = (*count as usize).min(tags.len() - start);
                for (i, &tag) in tags[start..start + len].iter().enumerate() {
                    unsafe { table_tags.add(i).write(tag_from_rust(tag)) };
                }
                *count = len as c_uint;
            }
        }
        return tags.len() as c_uint;
    }
    if let Some(count) = unsafe { table_count.as_mut() } {
        *count = 0;
    }
    0
}

/// Returns a face's design units per em, or 1000 if it has no `head` table.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_get_upem(face: *mut hr_face_t) -> c_uint {
    unsafe { object::or_empty(face.cast_const()) }.upem()
}

/// Returns the number of glyphs in a face.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_get_glyph_count(face: *mut hr_face_t) -> c_uint {
    unsafe { object::or_empty(face.cast_const()) }.glyph_count()
}

/// Returns the blob a face was created over, or the empty blob for a face
/// built from table callbacks.
///
/// The caller owns the returned reference.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_reference_blob(face: *mut hr_face_t) -> *mut hr_blob_t {
    let face = unsafe { object::or_empty(face.cast_const()) };
    match &face.source {
        FaceSource::Blob { blob, .. } => unsafe { object::reference(*blob) },
        FaceSource::Empty => hr_blob_t::empty(),
        // The whole font is what the none tag asks a table callback for,
        // which is how a face built from one hands its bytes back.
        FaceSource::Function(state) => match state.call(tag_to_rust(crate::common::HR_TAG_NONE)) {
            Some(data) => hr_blob_t::new(data),
            None => hr_blob_t::empty(),
        },
    }
}

/// Returns the data of one table, or the empty blob if the face has no such
/// table.
///
/// The caller owns the returned reference. For a blob-backed face the result
/// shares the face's storage rather than copying. For a face built from table
/// callbacks, this invokes the callback.
///
/// # Safety
///
/// `face` must be `NULL` or a live face.
#[no_mangle]
pub unsafe extern "C" fn hr_face_reference_table(
    face: *mut hr_face_t,
    tag: hr_tag_t,
) -> *mut hr_blob_t {
    let face_ref = unsafe { object::or_empty(face.cast_const()) };
    // The none tag names the whole font rather than a table in it, and is
    // `hr_face_reference_blob`'s business; there is no table to look up here.
    if tag == crate::common::HR_TAG_NONE {
        return hr_blob_t::empty();
    }
    match &face_ref.source {
        FaceSource::Empty => hr_blob_t::empty(),
        FaceSource::Function(state) => match state.call(tag_to_rust(tag)) {
            Some(data) => hr_blob_t::new(data),
            None => hr_blob_t::empty(),
        },
        FaceSource::Blob { blob, .. } => {
            let Some((_, directory)) = face_ref.source.blob_directory() else {
                return hr_blob_t::empty();
            };
            let Some((start, end)) = directory.range(tag_to_rust(tag)) else {
                return hr_blob_t::empty();
            };
            // `sub` answers a range outside the blob with the empty blob.
            // SAFETY: the face owns a reference to `blob`, so it is live.
            unsafe { hr_blob_t::sub(*blob, start, end) }
        }
    }
}

/// Adds the face's nominal cmap coverage to `out`, preserving existing entries.
///
/// Uses the selected cmap subtable's stored character codes, without the symbol
/// fallback or Mac Roman conversion applied by nominal glyph lookup. Variation
/// sequences are not included. A missing cmap adds nothing.
/// # Safety
/// `face` must be null or live; `out` must be null or live and must not be
/// accessed concurrently while it is being modified.
#[no_mangle]
pub unsafe extern "C" fn hr_face_collect_unicodes(face: *mut hr_face_t, out: *mut hr_set_t) {
    let face = unsafe { object::or_empty(face.cast_const()) };
    let Some(font) = face.font() else { return };
    let Some((_, _, subtable)) = font
        .tables()
        .cmap()
        .ok()
        .and_then(|table| table.best_subtable())
    else {
        return;
    };
    let Some(out) = (unsafe { object::as_mutable(out) }) else {
        return;
    };
    // A format 13 group maps an entire range to one glyph. Insert ranges
    // directly, both to bound malformed glyph IDs and to avoid iterating
    // over a million characters in a last-resort font.
    if let CmapSubtable::Format13(table) = subtable {
        // Groups are meant to be sorted and disjoint, but nothing enforces
        // that. Sort the accepted ranges and insert only what the earlier
        // ones did not already cover, so that a malformed table whose groups
        // overlap cannot insert the same range once per group.
        let mut ranges: Vec<(u32, u32)> = table
            .groups()
            .iter()
            .filter_map(|group| {
                let first = group.start_char_code();
                let last = group.end_char_code().min(0x0010_FFFF);
                let glyph = group.glyph_id();
                (first <= last && glyph != 0 && glyph < font.num_glyphs()).then_some((first, last))
            })
            .collect();
        ranges.sort_unstable();
        let mut next = 0;
        for (first, last) in ranges {
            let first = first.max(next);
            if first <= last {
                out.values.insert_range(first..=last);
                next = last + 1;
            }
        }
    } else {
        let limits = CmapIterLimits {
            // The iterator's character limit is exclusive.
            max_char: 0x0011_0000,
            glyph_count: font.num_glyphs(),
        };
        let is_format4 = matches!(subtable, CmapSubtable::Format4(_));
        for (codepoint, glyph) in subtable.iter_with_limits(limits) {
            // Format 4's final U+FFFF segment is a sentinel.
            if glyph.to_u32() != 0 && !(is_format4 && codepoint == 0xFFFF) {
                out.add(codepoint);
            }
        }
    }
}
