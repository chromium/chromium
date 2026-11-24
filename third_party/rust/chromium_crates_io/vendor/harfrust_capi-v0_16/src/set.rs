//! Sets of codepoints and glyph IDs used by layout queries.

use core::ffi::c_void;
use core::ops::{Deref, RangeInclusive};
use read_fonts::collections::IntSet;
use std::collections::BTreeMap;
use std::hash::{DefaultHasher, Hash, Hasher};
use std::sync::OnceLock;

use crate::common::{hr_bool_t, hr_codepoint_t, HR_CODEPOINT_INVALID};
use crate::object::{self, hr_destroy_func_t, hr_user_data_key_t, Empty, Object, ObjectHeader};

/// Unset set value, also used to start iteration.
pub const HR_SET_VALUE_INVALID: hr_codepoint_t = HR_CODEPOINT_INVALID;

pub struct hr_set_t {
    header: ObjectHeader,
    pub(crate) values: SetValues,
}

/// A lazy index of the underlying bitset's stored ranges. For inverted sets
/// these are exclusions, so inversion leaves the index unchanged. Mutations
/// update only affected entries instead of rebuilding a snapshot on each query.
#[derive(Default)]
pub(crate) struct SetValues {
    values: IntSet<u32>,
    ranges: OnceLock<BTreeMap<u32, u32>>,
}

#[derive(Clone, Copy)]
enum SetOperation {
    Union,
    Intersect,
    Subtract,
}

#[derive(Clone, Copy)]
enum IndexOperation {
    Union,
    Intersect,
    Subtract,
    ReverseSubtract,
}

fn index_insert(index: &mut BTreeMap<u32, u32>, mut first: u32, mut last: u32) {
    if let Some((&previous_first, &previous_last)) = index.range(..=first).next_back() {
        if previous_last >= last {
            return;
        }
        if previous_last >= first.saturating_sub(1) {
            first = previous_first;
            index.remove(&previous_first);
        }
    }
    while let Some((&next_first, &next_last)) = index.range(first..).next() {
        if next_first > last.saturating_add(1) {
            break;
        }
        last = last.max(next_last);
        index.remove(&next_first);
    }
    index.insert(first, last);
}

fn index_remove(index: &mut BTreeMap<u32, u32>, first: u32, last: u32) {
    while let Some((&range_first, &range_last)) = index.range(..=last).next_back() {
        if range_last < first {
            break;
        }
        index.remove(&range_first);
        if range_last > last {
            index.insert(last + 1, range_last);
        }
        if range_first < first {
            index.insert(range_first, first - 1);
            break;
        }
    }
}

fn index_operation(
    index: &mut BTreeMap<u32, u32>,
    ranges: impl Iterator<Item = RangeInclusive<u32>>,
    operation: IndexOperation,
) {
    match operation {
        IndexOperation::Union => {
            for range in ranges {
                index_insert(index, *range.start(), *range.end());
            }
        }
        IndexOperation::Subtract => {
            for range in ranges {
                index_remove(index, *range.start(), *range.end());
            }
        }
        IndexOperation::Intersect => {
            let mut first = 0;
            for range in ranges {
                if *range.start() > first {
                    index_remove(index, first, *range.start() - 1);
                }
                let Some(next) = range.end().checked_add(1) else {
                    return;
                };
                first = next;
            }
            index_remove(index, first, u32::MAX);
        }
        IndexOperation::ReverseSubtract => {
            let previous = core::mem::take(index);
            *index = ranges.map(|range| (*range.start(), *range.end())).collect();
            for (first, last) in previous {
                index_remove(index, first, last);
            }
        }
    }
}

impl SetValues {
    fn stored_ranges(&self) -> impl Iterator<Item = RangeInclusive<u32>> + '_ {
        // Keep the unused iterator absent: filtering it after iteration would
        // still scan ranges that are not part of the bitset's stored values.
        let (included, excluded) = if self.values.is_inverted() {
            (None, Some(self.values.iter_excluded_ranges()))
        } else {
            (Some(self.values.iter_ranges()), None)
        };
        included
            .into_iter()
            .flatten()
            .chain(excluded.into_iter().flatten())
    }

    fn indexed_ranges(&self) -> &BTreeMap<u32, u32> {
        self.ranges.get_or_init(|| {
            self.stored_ranges()
                .map(|range| (*range.start(), *range.end()))
                .collect()
        })
    }

    fn next_range(&self, mut first: u32) -> Option<RangeInclusive<u32>> {
        if first == HR_SET_VALUE_INVALID {
            return None;
        }
        let index = self.indexed_ranges();
        let previous = index.range(..=first).next_back();
        let last = if self.values.is_inverted() {
            if let Some((_, &excluded_last)) = previous.filter(|(_, last)| **last >= first) {
                first = excluded_last.checked_add(1)?;
                if first == HR_SET_VALUE_INVALID {
                    return None;
                }
            }
            index
                .range(first..)
                .next()
                .map_or(HR_SET_VALUE_INVALID - 1, |(&excluded_first, _)| {
                    excluded_first - 1
                })
        } else if let Some((_, &last)) = previous.filter(|(_, last)| **last >= first) {
            last
        } else {
            let (&next_first, &last) = index.range(first..).next()?;
            first = next_first;
            last
        };
        Some(first..=last.min(HR_SET_VALUE_INVALID - 1))
    }

    fn previous_range(&self, end: u32) -> Option<RangeInclusive<u32>> {
        let mut last = end.checked_sub(1)?;
        let index = self.indexed_ranges();
        let previous = index.range(..=last).next_back();
        let first = if self.values.is_inverted() {
            if let Some((&excluded_first, _)) = previous.filter(|(_, end)| **end >= last) {
                last = excluded_first.checked_sub(1)?;
            }
            index
                .range(..=last)
                .next_back()
                .map_or(0, |(_, &excluded_last)| excluded_last + 1)
        } else {
            let (&first, &range_last) = previous?;
            last = last.min(range_last);
            first
        };
        Some(first..=last)
    }

    fn update_range(&mut self, first: u32, last: u32, include: bool) {
        if let Some(index) = self.ranges.get_mut() {
            if include == self.values.is_inverted() {
                index_remove(index, first, last);
            } else {
                index_insert(index, first, last);
            }
        }
    }

    pub(crate) fn insert(&mut self, value: u32) -> bool {
        let changed = self.values.insert(value);
        if changed {
            self.update_range(value, value, true);
        }
        changed
    }

    pub(crate) fn remove(&mut self, value: u32) -> bool {
        let changed = self.values.remove(value);
        if changed {
            self.update_range(value, value, false);
        }
        changed
    }

    pub(crate) fn insert_range(&mut self, range: RangeInclusive<u32>) {
        let (first, last) = (*range.start(), *range.end());
        self.values.insert_range(range);
        if first <= last {
            self.update_range(first, last, true);
        }
    }

    pub(crate) fn remove_range(&mut self, range: RangeInclusive<u32>) {
        let (first, last) = (*range.start(), *range.end());
        self.values.remove_range(range);
        if first <= last {
            self.update_range(first, last, false);
        }
    }

    pub(crate) fn extend(&mut self, values: impl IntoIterator<Item = u32>) {
        if self.ranges.get().is_none() {
            self.values.extend(values);
            return;
        }
        for value in values {
            self.insert(value);
        }
    }

    pub(crate) fn clear(&mut self) {
        self.values.clear();
        if let Some(index) = self.ranges.get_mut() {
            index.clear();
        }
    }

    pub(crate) fn invert(&mut self) {
        self.values.invert();
    }

    fn operation(&mut self, other: &Self, operation: SetOperation) {
        use IndexOperation::{Intersect, ReverseSubtract, Subtract, Union};
        let index_op = match (
            operation,
            self.values.is_inverted(),
            other.values.is_inverted(),
        ) {
            (SetOperation::Union, false, false)
            | (SetOperation::Intersect, true, true)
            | (SetOperation::Subtract, true, false) => Union,
            (SetOperation::Union, true, true)
            | (SetOperation::Intersect, false, false)
            | (SetOperation::Subtract, false, true) => Intersect,
            (SetOperation::Union, true, false)
            | (SetOperation::Intersect, false, true)
            | (SetOperation::Subtract, false, false) => Subtract,
            (SetOperation::Union, false, true)
            | (SetOperation::Intersect, true, false)
            | (SetOperation::Subtract, true, true) => ReverseSubtract,
        };
        match operation {
            SetOperation::Union => self.values.union(&other.values),
            SetOperation::Intersect => self.values.intersect(&other.values),
            SetOperation::Subtract => self.values.subtract(&other.values),
        }
        if let Some(index) = self.ranges.get_mut() {
            index_operation(index, other.stored_ranges(), index_op);
        }
    }

    fn union(&mut self, other: &Self) {
        self.operation(other, SetOperation::Union);
    }
    fn intersect(&mut self, other: &Self) {
        self.operation(other, SetOperation::Intersect);
    }
    fn subtract(&mut self, other: &Self) {
        self.operation(other, SetOperation::Subtract);
    }
}

impl Clone for SetValues {
    fn clone(&self) -> Self {
        Self {
            values: self.values.clone(),
            ranges: OnceLock::new(),
        }
    }

    fn clone_from(&mut self, source: &Self) {
        self.values.clone_from(&source.values);
        self.ranges.take();
    }
}

// Deliberately read-only: every mutation must keep the range index in sync.
impl Deref for SetValues {
    type Target = IntSet<u32>;
    fn deref(&self) -> &Self::Target {
        &self.values
    }
}

static EMPTY_SET: OnceLock<Empty<hr_set_t>> = OnceLock::new();

impl Object for hr_set_t {
    fn header(&self) -> &ObjectHeader {
        &self.header
    }

    fn empty() -> *mut Self {
        EMPTY_SET
            .get_or_init(|| {
                Empty::new(hr_set_t {
                    header: ObjectHeader::immortal(),
                    values: SetValues::default(),
                })
            })
            .get()
    }
}

impl hr_set_t {
    // IntSet's domain includes the invalid value. Like HarfBuzz, we never
    // insert it into the underlying bitset, and omit it from enumeration
    // and population even though an inverted set answers true to has().
    fn ranges(&self) -> impl Iterator<Item = RangeInclusive<u32>> + '_ {
        self.values.iter_ranges().filter_map(|range| {
            let first = *range.start();
            let last = (*range.end()).min(HR_SET_VALUE_INVALID - 1);
            (first <= last).then_some(first..=last)
        })
    }

    fn next(&self, start: u32) -> u32 {
        if start == HR_SET_VALUE_INVALID {
            return HR_SET_VALUE_INVALID;
        }
        if !self.values.is_inverted() {
            return self
                .values
                .range(start..)
                .next()
                .unwrap_or(HR_SET_VALUE_INVALID);
        }
        // Nearby members need neither an allocation nor a range scan. The
        // dependency's inverted range(start..) walks exclusions one by one,
        // while iter_ranges() rescans the preceding pages. Cache its ranges
        // only when this bounded probe does not find the next member.
        for value in start..=start.saturating_add(7).min(HR_SET_VALUE_INVALID - 1) {
            if self.values.contains(value) {
                return value;
            }
        }
        self.values
            .next_range(start)
            .map_or(HR_SET_VALUE_INVALID, |range| *range.start())
    }

    fn previous(&self, end: u32) -> u32 {
        let Some(value) = end.checked_sub(1) else {
            return HR_SET_VALUE_INVALID;
        };
        if self.values.contains(value) {
            return value;
        }
        if !self.values.is_inverted() {
            if let Some(last) = self.values.last().filter(|last| *last < end) {
                return last;
            }
        }
        self.values
            .previous_range(end)
            .map_or(HR_SET_VALUE_INVALID, |range| *range.end())
    }

    fn next_range(&self, start: u32) -> Option<RangeInclusive<u32>> {
        let first = self.next(start);
        if first == HR_SET_VALUE_INVALID {
            return None;
        }
        for last in first..=first.saturating_add(7).min(HR_SET_VALUE_INVALID - 1) {
            if last == HR_SET_VALUE_INVALID - 1 || !self.values.contains(last + 1) {
                return Some(first..=last);
            }
        }
        self.values.next_range(first)
    }

    fn previous_range(&self, end: u32) -> Option<RangeInclusive<u32>> {
        let last = self.previous(end);
        if last == HR_SET_VALUE_INVALID {
            return None;
        }
        for first in (last.saturating_sub(7)..=last).rev() {
            if first == 0 || !self.values.contains(first - 1) {
                return Some(first..=last);
            }
        }
        self.values.previous_range(last + 1)
    }

    pub(crate) fn add(&mut self, value: u32) {
        if value != HR_CODEPOINT_INVALID {
            self.values.insert(value);
        }
    }
}

#[no_mangle]
pub extern "C" fn hr_set_create() -> *mut hr_set_t {
    object::create(hr_set_t {
        header: ObjectHeader::new(),
        values: SetValues::default(),
    })
}

#[no_mangle]
pub extern "C" fn hr_set_get_empty() -> *mut hr_set_t {
    hr_set_t::empty()
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_reference(set: *mut hr_set_t) -> *mut hr_set_t {
    unsafe { object::reference(set) }
}

/// # Safety
/// `set` must be `NULL` or live, and the caller must own its reference.
#[no_mangle]
pub unsafe extern "C" fn hr_set_destroy(set: *mut hr_set_t) {
    unsafe { object::destroy(set) };
}

/// # Safety
/// `set` must be `NULL` or live, and `key` must outlive it.
#[no_mangle]
pub unsafe extern "C" fn hr_set_set_user_data(
    set: *mut hr_set_t,
    key: *const hr_user_data_key_t,
    data: *mut c_void,
    destroy: hr_destroy_func_t,
    replace: hr_bool_t,
) -> hr_bool_t {
    unsafe { object::set_user_data(set, key, data, destroy, replace != 0) }.into()
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_get_user_data(
    set: *const hr_set_t,
    key: *const hr_user_data_key_t,
) -> *mut c_void {
    unsafe { object::get_user_data(set.cast_mut(), key) }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_clear(set: *mut hr_set_t) {
    if let Some(set) = unsafe { object::as_mutable(set) } {
        set.values.clear();
    }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_add(set: *mut hr_set_t, value: hr_codepoint_t) {
    if let Some(set) = unsafe { object::as_mutable(set) } {
        set.add(value);
    }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_has(set: *const hr_set_t, value: hr_codepoint_t) -> hr_bool_t {
    let set = unsafe { object::or_empty(set) };
    set.values.contains(value).into()
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_is_empty(set: *const hr_set_t) -> hr_bool_t {
    let set = unsafe { object::or_empty(set) };
    (unsafe { hr_set_get_population(set) } == 0).into()
}

/// Returns false for the inert empty singleton. Rust allocation failures abort.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_allocation_successful(set: *const hr_set_t) -> hr_bool_t {
    (!unsafe { object::is_immutable(object::or_empty(set)) }).into()
}

/// Copies the contents, without copying user data.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_copy(set: *const hr_set_t) -> *mut hr_set_t {
    object::create(hr_set_t {
        header: ObjectHeader::new(),
        values: unsafe { object::or_empty(set) }.values.clone(),
    })
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_invert(set: *mut hr_set_t) {
    if let Some(set) = unsafe { object::as_mutable(set) } {
        set.values.invert();
    }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_is_inverted(set: *const hr_set_t) -> hr_bool_t {
    unsafe { object::or_empty(set) }.values.is_inverted().into()
}

/// Adds the inclusive range. Reversed ranges are ignored. An invalid `last`
/// is ignored for ordinary sets and extends to the end for inverted sets.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_add_range(
    set: *mut hr_set_t,
    first: hr_codepoint_t,
    last: hr_codepoint_t,
) {
    if let Some(set) = unsafe { object::as_mutable(set) } {
        if first <= last
            && first != HR_SET_VALUE_INVALID
            && (last != HR_SET_VALUE_INVALID || set.values.is_inverted())
        {
            set.values
                .insert_range(first..=last.min(HR_SET_VALUE_INVALID - 1));
        }
    }
}

/// Adds codepoints supplied in increasing order.
///
/// # Safety
/// `set` must be `NULL` or live. `sorted_codepoints` must point to
/// `num_codepoints` readable elements, or be `NULL` when the count is zero.
#[no_mangle]
pub unsafe extern "C" fn hr_set_add_sorted_array(
    set: *mut hr_set_t,
    sorted_codepoints: *const hr_codepoint_t,
    num_codepoints: core::ffi::c_uint,
) {
    if num_codepoints == 0 || sorted_codepoints.is_null() {
        return;
    }
    if let Some(set) = unsafe { object::as_mutable(set) } {
        let values =
            unsafe { core::slice::from_raw_parts(sorted_codepoints, num_codepoints as usize) };
        set.values.extend(
            values
                .iter()
                .copied()
                .filter(|&v| v != HR_SET_VALUE_INVALID),
        );
    }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_del(set: *mut hr_set_t, value: hr_codepoint_t) {
    if value != HR_SET_VALUE_INVALID {
        if let Some(set) = unsafe { object::as_mutable(set) } {
            set.values.remove(value);
        }
    }
}

/// Deletes the inclusive range. An invalid `last` extends to the end for
/// ordinary sets and is ignored for inverted sets.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_del_range(
    set: *mut hr_set_t,
    first: hr_codepoint_t,
    last: hr_codepoint_t,
) {
    if let Some(set) = unsafe { object::as_mutable(set) } {
        if first <= last
            && first != HR_SET_VALUE_INVALID
            && (last != HR_SET_VALUE_INVALID || !set.values.is_inverted())
        {
            set.values
                .remove_range(first..=last.min(HR_SET_VALUE_INVALID - 1));
        }
    }
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_is_equal(
    set: *const hr_set_t,
    other: *const hr_set_t,
) -> hr_bool_t {
    let set = unsafe { object::or_empty(set) };
    let other = unsafe { object::or_empty(other) };
    set.ranges().eq(other.ranges()).into()
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_intersects(
    set: *const hr_set_t,
    other: *const hr_set_t,
) -> hr_bool_t {
    let set = unsafe { object::or_empty(set) };
    let other = unsafe { object::or_empty(other) };
    let mut a = set.ranges();
    let mut b = other.ranges();
    let (mut x, mut y) = (a.next(), b.next());
    while let (Some(ar), Some(br)) = (&x, &y) {
        if ar.end() < br.start() {
            x = a.next();
        } else if br.end() < ar.start() {
            y = b.next();
        } else {
            return 1;
        }
    }
    0
}

/// Equal sets have equal hashes. The hash value is implementation dependent.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_hash(set: *const hr_set_t) -> core::ffi::c_uint {
    let mut hash = DefaultHasher::new();
    for range in unsafe { object::or_empty(set) }.ranges() {
        range.hash(&mut hash);
    }
    hash.finish() as core::ffi::c_uint
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_is_subset(
    set: *const hr_set_t,
    larger_set: *const hr_set_t,
) -> hr_bool_t {
    let set = unsafe { object::or_empty(set) };
    let larger = unsafe { object::or_empty(larger_set) };
    let mut larger_ranges = larger.ranges();
    let mut covering = larger_ranges.next();
    for range in set.ranges() {
        while covering.as_ref().is_some_and(|r| r.end() < range.start()) {
            covering = larger_ranges.next();
        }
        if !covering
            .as_ref()
            .is_some_and(|r| r.start() <= range.start() && r.end() >= range.end())
        {
            return 0;
        }
    }
    1
}

// Check pointer identity before making references: the C API permits self
// assignment and self operations, but Rust cannot alias &mut and &.
unsafe fn combine(
    set: *mut hr_set_t,
    other: *const hr_set_t,
    op: fn(&mut SetValues, &SetValues),
    subtract_self: bool,
) {
    let other = if other.is_null() {
        hr_set_t::empty().cast_const()
    } else {
        other
    };
    if core::ptr::eq(set, other) {
        if subtract_self {
            unsafe { hr_set_clear(set) };
        }
        return;
    }
    let other = unsafe { object::or_empty(other) };
    if let Some(set) = unsafe { object::as_mutable(set) } {
        op(&mut set.values, &other.values);
    }
}

/// Replaces the contents, retaining the destination's user data.
///
/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_set(set: *mut hr_set_t, other: *const hr_set_t) {
    unsafe { combine(set, other, SetValues::clone_from, false) };
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_union(set: *mut hr_set_t, other: *const hr_set_t) {
    unsafe { combine(set, other, SetValues::union, false) };
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_intersect(set: *mut hr_set_t, other: *const hr_set_t) {
    unsafe { combine(set, other, SetValues::intersect, false) };
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_subtract(set: *mut hr_set_t, other: *const hr_set_t) {
    unsafe { combine(set, other, SetValues::subtract, true) };
}

/// # Safety
/// Both sets must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_symmetric_difference(set: *mut hr_set_t, other: *const hr_set_t) {
    unsafe {
        combine(
            set,
            other,
            |a, b| {
                let mut rhs = b.clone();
                rhs.subtract(a);
                a.subtract(b);
                a.union(&rhs);
            },
            true,
        );
    }
}

/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_get_population(set: *const hr_set_t) -> core::ffi::c_uint {
    let values = &unsafe { object::or_empty(set) }.values;
    (values.len() - u64::from(values.is_inverted())) as core::ffi::c_uint
}

/// Returns the smallest value, or `HR_SET_VALUE_INVALID` if empty.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_get_min(set: *const hr_set_t) -> hr_codepoint_t {
    unsafe { object::or_empty(set) }.next(0)
}

/// Returns the largest value, or `HR_SET_VALUE_INVALID` if empty.
///
/// # Safety
/// `set` must be `NULL` or live.
#[no_mangle]
pub unsafe extern "C" fn hr_set_get_max(set: *const hr_set_t) -> hr_codepoint_t {
    unsafe { object::or_empty(set) }.previous(HR_SET_VALUE_INVALID)
}

/// Finds the next value. Start with `HR_SET_VALUE_INVALID`; exhaustion writes
/// that value again.
///
/// # Safety
/// `set` must be `NULL` or live. `codepoint` must be writable.
#[no_mangle]
pub unsafe extern "C" fn hr_set_next(
    set: *const hr_set_t,
    codepoint: *mut hr_codepoint_t,
) -> hr_bool_t {
    let Some(codepoint) = (unsafe { codepoint.as_mut() }) else {
        return 0;
    };
    let start = codepoint.wrapping_add(1);
    *codepoint = unsafe { object::or_empty(set) }.next(start);
    (*codepoint != HR_SET_VALUE_INVALID).into()
}

/// Finds the preceding value. Start with `HR_SET_VALUE_INVALID`; exhaustion
/// writes that value again.
///
/// # Safety
/// `set` must be `NULL` or live. `codepoint` must be writable.
#[no_mangle]
pub unsafe extern "C" fn hr_set_previous(
    set: *const hr_set_t,
    codepoint: *mut hr_codepoint_t,
) -> hr_bool_t {
    let Some(codepoint) = (unsafe { codepoint.as_mut() }) else {
        return 0;
    };
    *codepoint = unsafe { object::or_empty(set) }.previous(*codepoint);
    (*codepoint != HR_SET_VALUE_INVALID).into()
}

/// Finds the next contiguous range after `*last`. Start with
/// `HR_SET_VALUE_INVALID`; exhaustion writes it to both outputs.
///
/// # Safety
/// `set` must be `NULL` or live. `first` and `last` must be writable.
#[no_mangle]
pub unsafe extern "C" fn hr_set_next_range(
    set: *const hr_set_t,
    first: *mut hr_codepoint_t,
    last: *mut hr_codepoint_t,
) -> hr_bool_t {
    if first.is_null() || last.is_null() {
        return 0;
    }
    let start = unsafe { *last }.wrapping_add(1);
    let range = unsafe { object::or_empty(set) }.next_range(start);
    let (a, b) = range.map_or((HR_SET_VALUE_INVALID, HR_SET_VALUE_INVALID), |r| {
        (*r.start(), *r.end())
    });
    unsafe {
        *first = a;
        *last = b;
    }
    (a != HR_SET_VALUE_INVALID).into()
}

/// Finds the preceding contiguous range before `*first`. Start with
/// `HR_SET_VALUE_INVALID`; exhaustion writes it to both outputs.
///
/// # Safety
/// `set` must be `NULL` or live. `first` and `last` must be writable.
#[no_mangle]
pub unsafe extern "C" fn hr_set_previous_range(
    set: *const hr_set_t,
    first: *mut hr_codepoint_t,
    last: *mut hr_codepoint_t,
) -> hr_bool_t {
    if first.is_null() || last.is_null() {
        return 0;
    }
    let end = unsafe { *first };
    let range = unsafe { object::or_empty(set) }.previous_range(end);
    let (a, b) = range.map_or((HR_SET_VALUE_INVALID, HR_SET_VALUE_INVALID), |r| {
        (*r.start(), *r.end())
    });
    unsafe {
        *first = a;
        *last = b;
    }
    (a != HR_SET_VALUE_INVALID).into()
}

/// Writes at most `size` values after `codepoint`. Start with
/// `HR_SET_VALUE_INVALID`. Returns the number written.
///
/// # Safety
/// `set` must be `NULL` or live. `out` must point to `size` writable elements,
/// or be `NULL` when `size` is zero.
#[no_mangle]
pub unsafe extern "C" fn hr_set_next_many(
    set: *const hr_set_t,
    codepoint: hr_codepoint_t,
    out: *mut hr_codepoint_t,
    size: core::ffi::c_uint,
) -> core::ffi::c_uint {
    if size == 0 || out.is_null() {
        return 0;
    }
    let set = unsafe { object::or_empty(set) };
    let out = unsafe { core::slice::from_raw_parts_mut(out, size as usize) };
    let mut count = 0;
    let mut value = codepoint;
    for slot in out.iter_mut() {
        value = set.next(value.wrapping_add(1));
        if value == HR_SET_VALUE_INVALID {
            break;
        }
        *slot = value;
        count += 1;
    }
    count
}
