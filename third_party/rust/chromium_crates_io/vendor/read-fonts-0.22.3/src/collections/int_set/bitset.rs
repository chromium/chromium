//! A fast & efficient ordered set for unsigned integers.

use super::bitpage::BitPage;
use super::bitpage::RangeIter;
use super::bitpage::PAGE_BITS;
use std::cell::Cell;
use std::cmp::Ordering;
use std::hash::Hash;

use std::ops::RangeInclusive;

// log_2(PAGE_BITS)
const PAGE_BITS_LOG_2: u32 = PAGE_BITS.ilog2();

/// An ordered integer (u32) set.
#[derive(Clone, Debug)]
pub(crate) struct BitSet {
    // TODO(garretrieger): consider a "small array" type instead of Vec.
    pages: Vec<BitPage>,
    page_map: Vec<PageInfo>,
    len: Cell<u64>, // TODO(garretrieger): use an option instead of a sentinel.
}

impl BitSet {
    /// Add val as a member of this set.
    pub(crate) fn insert(&mut self, val: u32) -> bool {
        let page = self.ensure_page_for_mut(val);
        let ret = page.insert(val);
        self.mark_dirty();
        ret
    }

    /// Add all values in range as members of this set.
    pub(crate) fn insert_range(&mut self, range: RangeInclusive<u32>) {
        let start = *range.start();
        let end = *range.end();
        if start > end {
            return;
        }

        let major_start = Self::get_major_value(start);
        let major_end = Self::get_major_value(end);

        for major in major_start..=major_end {
            let page_start = start.max(Self::major_start(major));
            let page_end = end.min(Self::major_start(major) + (PAGE_BITS - 1));
            let page = self.ensure_page_for_major_mut(major);
            page.insert_range(page_start, page_end);
        }
        self.mark_dirty();
    }

    /// An alternate version of [`extend()`] which is optimized for inserting an unsorted
    /// iterator of values.
    ///
    /// [`extend()`]: Self::extend
    pub(crate) fn extend_unsorted<U: IntoIterator<Item = u32>>(&mut self, iter: U) {
        for val in iter {
            let major_value = Self::get_major_value(val);
            let page = self.ensure_page_for_major_mut(major_value);
            page.insert_no_return(val);
        }
        self.mark_dirty();
    }

    /// Remove val from this set.
    pub(crate) fn remove(&mut self, val: u32) -> bool {
        let maybe_page = self.page_for_mut(val);
        if let Some(page) = maybe_page {
            let ret = page.remove(val);
            self.mark_dirty();
            ret
        } else {
            false
        }
    }

    // Remove all values in iter from this set.
    pub(crate) fn remove_all<U: IntoIterator<Item = u32>>(&mut self, iter: U) {
        let mut last_page_index: Option<usize> = None;
        let mut last_major_value = u32::MAX;
        for val in iter {
            let major_value = Self::get_major_value(val);
            if major_value != last_major_value {
                last_page_index = self.page_index_for_major(major_value);
                last_major_value = major_value;
            };

            let Some(page_index) = last_page_index else {
                continue;
            };

            if let Some(page) = self.pages.get_mut(page_index) {
                page.remove(val);
            }
        }
        self.mark_dirty();
    }

    /// Removes all values in range as members of this set.
    pub(crate) fn remove_range(&mut self, range: RangeInclusive<u32>) {
        let start = *(range.start());
        let end = *(range.end());
        if start > end {
            return;
        }

        let start_major = Self::get_major_value(start);
        let end_major = Self::get_major_value(end);
        let mut info_index = match self
            .page_map
            .binary_search_by(|probe| probe.major_value.cmp(&start_major))
        {
            Ok(info_index) => info_index,
            Err(info_index) => info_index,
        };

        loop {
            let Some(info) = self.page_map.get(info_index) else {
                break;
            };
            let Some(page) = self.pages.get_mut(info.index as usize) else {
                break;
            };

            if info.major_value > end_major {
                break;
            } else if info.major_value == start_major {
                page.remove_range(start, Self::major_end(start_major).min(end));
            } else if info.major_value == end_major {
                page.remove_range(Self::major_start(end_major), end);
                break;
            } else {
                page.clear();
            }
            info_index += 1;
        }

        self.mark_dirty();
    }

    /// Returns true if val is a member of this set.
    pub(crate) fn contains(&self, val: u32) -> bool {
        self.page_for(val)
            .map(|page| page.contains(val))
            .unwrap_or(false)
    }

    pub(crate) fn empty() -> BitSet {
        BitSet {
            pages: vec![],
            page_map: vec![],
            len: Default::default(),
        }
    }

    /// Remove all members from this set.
    pub(crate) fn clear(&mut self) {
        self.pages.clear();
        self.page_map.clear();
        self.mark_dirty();
    }

    /// Return true if there are no members in this set.
    #[cfg(test)]
    pub(crate) fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the number of members in this set.
    pub(crate) fn len(&self) -> u64 {
        // TODO(garretrieger): keep track of len on the fly, rather than computing it. Leave a computation method
        //                     for complex cases if needed.
        if self.is_dirty() {
            // this means we're stale and should recompute
            let len = self.pages.iter().map(|val| val.len() as u64).sum();
            self.len.set(len);
        }
        self.len.get()
    }

    pub(crate) fn num_pages(&self) -> usize {
        self.pages.len()
    }

    /// Sets the members of this set to the union of self and other.
    pub(crate) fn union(&mut self, other: &BitSet) {
        self.process(BitPage::union, other);
    }

    /// Sets the members of this set to the intersection of self and other.
    pub(crate) fn intersect(&mut self, other: &BitSet) {
        self.process(BitPage::intersect, other);
    }

    /// Sets the members of this set to self - other.
    pub(crate) fn subtract(&mut self, other: &BitSet) {
        self.process(BitPage::subtract, other);
    }

    /// Sets the members of this set to other - self.
    pub(crate) fn reversed_subtract(&mut self, other: &BitSet) {
        self.process(|a, b| BitPage::subtract(b, a), other);
    }

    pub(crate) fn iter(&self) -> impl DoubleEndedIterator<Item = u32> + '_ {
        self.iter_non_empty_pages().flat_map(|(major, page)| {
            let base = Self::major_start(major);
            page.iter().map(move |v| base + v)
        })
    }

    /// Iterator over the members of this set that come after `value`.
    pub(crate) fn iter_after(&self, value: u32) -> impl Iterator<Item = u32> + '_ {
        let major_value = Self::get_major_value(value);
        let result = self
            .page_map
            .binary_search_by(|probe| probe.major_value.cmp(&major_value));

        let (page_map_index, partial_first_page) = match result {
            Ok(page_map_index) => (page_map_index, true),
            Err(page_map_index) => (page_map_index, false),
        };

        let page = self
            .page_map
            .get(page_map_index)
            .and_then(move |page_info| {
                self.pages
                    .get(page_info.index as usize)
                    .map(|page| (page, page_info.major_value))
            });

        let init_it =
            page.filter(|_| partial_first_page)
                .into_iter()
                .flat_map(move |(page, major)| {
                    let base = Self::major_start(major);
                    page.iter_after(value).map(move |v| base + v)
                });

        let follow_on_page_map_index = if partial_first_page {
            page_map_index + 1
        } else {
            page_map_index
        };

        let follow_on_it = self.page_map[follow_on_page_map_index..]
            .iter()
            .flat_map(|info| {
                self.pages
                    .get(info.index as usize)
                    .map(|page| (info.major_value, page))
            })
            .filter(|(_, page)| !page.is_empty())
            .flat_map(|(major, page)| {
                let base = Self::major_start(major);
                page.iter().map(move |v| base + v)
            });

        init_it.chain(follow_on_it)
    }

    pub(crate) fn iter_ranges(&self) -> impl Iterator<Item = RangeInclusive<u32>> + '_ {
        BitSetRangeIter::new(self)
    }

    fn iter_pages(&self) -> impl DoubleEndedIterator<Item = (u32, &BitPage)> + '_ {
        self.page_map.iter().flat_map(|info| {
            self.pages
                .get(info.index as usize)
                .map(|page| (info.major_value, page))
        })
    }

    fn iter_non_empty_pages(&self) -> impl DoubleEndedIterator<Item = (u32, &BitPage)> + '_ {
        self.iter_pages().filter(|(_, page)| !page.is_empty())
    }

    /// Determine the passthrough behaviour of the operator.
    ///
    /// The passthrough behaviour is what happens to a page on one side of the operation if the other side is 0.
    /// For example union passes through both left and right sides since it preserves the left or right side when
    /// the other side is 0. Knowing this lets us optimize some cases when only one page is present on one side.
    fn passthrough_behavior<Op>(op: &Op) -> (bool, bool)
    where
        Op: Fn(&BitPage, &BitPage) -> BitPage,
    {
        let mut one: BitPage = BitPage::new_zeroes();
        one.insert(0);
        let zero: BitPage = BitPage::new_zeroes();

        let passthrough_left: bool = op(&one, &zero).contains(0);
        let passthrough_right: bool = op(&zero, &one).contains(0);

        (passthrough_left, passthrough_right)
    }

    fn process<Op>(&mut self, op: Op, other: &BitSet)
    where
        Op: Fn(&BitPage, &BitPage) -> BitPage,
    {
        let (passthrough_left, passthrough_right) = BitSet::passthrough_behavior(&op);

        self.mark_dirty();

        let mut len_a = self.pages.len();
        let len_b = other.pages.len();
        let mut idx_a = 0;
        let mut idx_b = 0;
        let mut count = 0;
        let mut write_idx = 0;

        // Step 1: Estimate the new size of this set (in number of pages) after processing, and remove left side
        //         pages that won't be needed.
        while idx_a < len_a && idx_b < len_b {
            let a_major = self.page_map[idx_a].major_value;
            let b_major = other.page_map[idx_b].major_value;

            match a_major.cmp(&b_major) {
                Ordering::Equal => {
                    if !passthrough_left {
                        // If we don't passthrough the left side, then the only case where we
                        // keep a page from the left is when there is also a page at the same major
                        // on the right side. In this case move page_map entries that we're keeping
                        // on the left side set to the front of the page_map vector. Otherwise if
                        // we do passthrough left, then we we keep all left hand side pages and this
                        // isn't necessary.
                        if write_idx < idx_a {
                            self.page_map[write_idx] = self.page_map[idx_a];
                        }
                        write_idx += 1;
                    }

                    count += 1;
                    idx_a += 1;
                    idx_b += 1;
                }
                Ordering::Less => {
                    if passthrough_left {
                        count += 1;
                    }
                    idx_a += 1;
                }
                Ordering::Greater => {
                    if passthrough_right {
                        count += 1;
                    }
                    idx_b += 1;
                }
            }
        }

        if passthrough_left {
            count += len_a - idx_a;
        }

        if passthrough_right {
            count += len_b - idx_b;
        }

        // Step 2: compact and resize for the new estimated left side size.
        let mut next_page = len_a;
        if !passthrough_left {
            len_a = write_idx;
            next_page = write_idx;
            self.compact(write_idx);
        }

        self.resize(count);
        let new_count = count;

        // Step 3: process and apply op in-place from the last to first page.
        idx_a = len_a;
        idx_b = len_b;
        while idx_a > 0 && idx_b > 0 {
            match self.page_map[idx_a - 1]
                .major_value
                .cmp(&other.page_map[idx_b - 1].major_value)
            {
                Ordering::Equal => {
                    idx_a -= 1;
                    idx_b -= 1;
                    count -= 1;
                    self.page_map[count] = self.page_map[idx_a];
                    *self.page_for_index_mut(count).unwrap() = op(
                        self.page_for_index(idx_a).unwrap(),
                        other.page_for_index(idx_b).unwrap(),
                    );
                }
                Ordering::Greater => {
                    idx_a -= 1;
                    if passthrough_left {
                        count -= 1;
                        self.page_map[count] = self.page_map[idx_a];
                    }
                }
                Ordering::Less => {
                    idx_b -= 1;
                    if passthrough_right {
                        count -= 1;
                        self.page_map[count].major_value = other.page_map[idx_b].major_value;
                        self.page_map[count].index = next_page as u32;
                        next_page += 1;
                        *self.page_for_index_mut(count).unwrap() =
                            other.page_for_index(idx_b).unwrap().clone();
                    }
                }
            }
        }

        // Step 4: there are only pages left on one side now, finish processing them if the appropriate passthrough is
        //         enabled.
        if passthrough_left {
            while idx_a > 0 {
                idx_a -= 1;
                count -= 1;
                self.page_map[count] = self.page_map[idx_a];
            }
        }

        if passthrough_right {
            while idx_b > 0 {
                idx_b -= 1;
                count -= 1;
                self.page_map[count].major_value = other.page_map[idx_b].major_value;
                self.page_map[count].index = next_page as u32;
                next_page += 1;
                *self.page_for_index_mut(count).unwrap() =
                    other.page_for_index(idx_b).unwrap().clone();
            }
        }

        self.resize(new_count);
    }

    fn compact(&mut self, new_len: usize) {
        let mut old_index_to_page_map_index = Vec::<usize>::with_capacity(self.pages.len());
        old_index_to_page_map_index.resize(self.pages.len(), usize::MAX);

        for i in 0usize..new_len {
            old_index_to_page_map_index[self.page_map[i].index as usize] = i;
        }

        self.compact_pages(old_index_to_page_map_index);
    }

    fn compact_pages(&mut self, old_index_to_page_map_index: Vec<usize>) {
        let mut write_index = 0;
        for (i, page_map_index) in old_index_to_page_map_index
            .iter()
            .enumerate()
            .take(self.pages.len())
        {
            if *page_map_index == usize::MAX {
                continue;
            }

            if write_index < i {
                self.pages[write_index] = self.pages[i].clone();
            }

            self.page_map[*page_map_index].index = write_index as u32;
            write_index += 1;
        }
    }

    fn resize(&mut self, new_len: usize) {
        self.page_map.resize(
            new_len,
            PageInfo {
                major_value: 0,
                index: 0,
            },
        );
        self.pages.resize(new_len, BitPage::new_zeroes());
    }

    fn mark_dirty(&mut self) {
        self.len.set(u64::MAX);
    }

    fn is_dirty(&self) -> bool {
        self.len.get() == u64::MAX
    }

    /// Return the major value (top 23 bits) of the page associated with value.
    const fn get_major_value(value: u32) -> u32 {
        value >> PAGE_BITS_LOG_2
    }

    const fn major_start(major: u32) -> u32 {
        major << PAGE_BITS_LOG_2
    }

    const fn major_end(major: u32) -> u32 {
        // Note: (PAGE_BITS - 1) must be grouped to prevent overflow on addition for the largest page.
        Self::major_start(major) + (PAGE_BITS - 1)
    }

    /// Returns the index in `self.pages` (if it exists) for the page with the same major as `major_value`.
    fn page_index_for_major(&self, major_value: u32) -> Option<usize> {
        self.page_map
            .binary_search_by(|probe| probe.major_value.cmp(&major_value))
            .ok()
            .map(|info_idx| self.page_map[info_idx].index as usize)
    }

    /// Returns the index in `self.pages` for the page with the same major as `major_value`. Will create
    /// the page if it does not yet exist.
    fn ensure_page_index_for_major(&mut self, major_value: u32) -> usize {
        match self
            .page_map
            .binary_search_by(|probe| probe.major_value.cmp(&major_value))
        {
            Ok(map_index) => self.page_map[map_index].index as usize,
            Err(map_index_to_insert) => {
                let page_index = self.pages.len();
                self.pages.push(BitPage::new_zeroes());
                let new_info = PageInfo {
                    index: page_index as u32,
                    major_value,
                };
                self.page_map.insert(map_index_to_insert, new_info);
                page_index
            }
        }
    }

    /// Return a reference to the page that `value` resides in.
    fn page_for(&self, value: u32) -> Option<&BitPage> {
        let major_value = Self::get_major_value(value);
        let pages_index = self.page_index_for_major(major_value)?;
        self.pages.get(pages_index)
    }

    /// Return a mutable reference to the page that `value` resides in.
    ///
    /// Insert a new page if it doesn't exist.
    fn page_for_mut(&mut self, value: u32) -> Option<&mut BitPage> {
        let major_value = Self::get_major_value(value);
        return self.page_for_major_mut(major_value);
    }

    /// Return a mutable reference to the page with major value equal to `major_value`.
    fn page_for_major_mut(&mut self, major_value: u32) -> Option<&mut BitPage> {
        let page_index = self.page_index_for_major(major_value)?;
        self.pages.get_mut(page_index)
    }

    /// Return a mutable reference to the page that `value` resides in.
    ///
    /// Insert a new page if it doesn't exist.
    fn ensure_page_for_mut(&mut self, value: u32) -> &mut BitPage {
        self.ensure_page_for_major_mut(Self::get_major_value(value))
    }

    /// Return a mutable reference to the page with major value equal to `major_value`.
    /// Inserts a new page if it doesn't exist.
    fn ensure_page_for_major_mut(&mut self, major_value: u32) -> &mut BitPage {
        let page_index = self.ensure_page_index_for_major(major_value);
        self.pages.get_mut(page_index).unwrap()
    }

    /// Return the mutable page at a given index
    fn page_for_index_mut(&mut self, index: usize) -> Option<&mut BitPage> {
        self.page_map
            .get(index)
            .and_then(|info| self.pages.get_mut(info.index as usize))
    }

    fn page_for_index(&self, index: usize) -> Option<&BitPage> {
        self.page_map
            .get(index)
            .and_then(|info| self.pages.get(info.index as usize))
    }
}

impl Extend<u32> for BitSet {
    fn extend<U: IntoIterator<Item = u32>>(&mut self, iter: U) {
        let mut builder = BitSetBuilder::start(self);
        for val in iter {
            builder.insert(val);
        }
        builder.finish();
    }
}

/// This helper is used to construct [`BitSet`]'s from a stream of possibly sorted values.
/// It remembers the last page index to reduce the amount of page lookups needed when inserting
/// sorted data. If given unsorted values it will still work correctly, but may be slower then just
/// repeatedly calling `insert()` on the bitset.
pub(crate) struct BitSetBuilder<'a> {
    pub(crate) set: &'a mut BitSet,
    last_page_index: usize,
    last_major_value: u32,
}

impl<'a> BitSetBuilder<'a> {
    pub(crate) fn start(set: &'a mut BitSet) -> Self {
        Self {
            set,
            last_page_index: usize::MAX,
            last_major_value: u32::MAX,
        }
    }

    pub(crate) fn insert(&mut self, val: u32) {
        // TODO(garretrieger): additional optimization ideas:
        // - Assuming data is sorted accumulate a single element mask and only commit it to the element
        //   once the next value passes the end of the element.
        let major_value = BitSet::get_major_value(val);
        if major_value != self.last_major_value {
            self.last_page_index = self.set.ensure_page_index_for_major(major_value);
            self.last_major_value = major_value;
        };
        if let Some(page) = self.set.pages.get_mut(self.last_page_index) {
            page.insert_no_return(val);
        }
    }

    pub(crate) fn finish(&mut self) {
        self.set.mark_dirty();
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct PageInfo {
    // index into pages vector of this page
    index: u32,
    /// the top 23 bits of values covered by this page
    major_value: u32,
}

impl Hash for BitSet {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.iter_non_empty_pages().for_each(|t| t.hash(state));
    }
}

impl std::cmp::PartialEq for BitSet {
    fn eq(&self, other: &Self) -> bool {
        let mut this = self.iter_non_empty_pages();
        let mut other = other.iter_non_empty_pages();

        // Note: normally we would prefer to use zip, but we also
        //       need to check that both iters have the same length.
        loop {
            match (this.next(), other.next()) {
                (Some(a), Some(b)) if a == b => continue,
                (None, None) => return true,
                _ => return false,
            }
        }
    }
}

impl std::cmp::Eq for BitSet {}

impl std::cmp::PartialOrd for BitSet {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl std::cmp::Ord for BitSet {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        let this_it = self.iter();
        let other_it = other.iter();

        for (us, them) in this_it.zip(other_it) {
            match us.cmp(&them) {
                core::cmp::Ordering::Equal => continue,
                other => return other,
            }
        }

        // all items in iter are the same: is one collection longer?
        self.len().cmp(&other.len())
    }
}

struct BitSetRangeIter<'a> {
    set: &'a BitSet,
    page_info_index: usize,
    page_iter: Option<RangeIter<'a>>,
}

impl<'a> BitSetRangeIter<'a> {
    fn new(set: &'a BitSet) -> BitSetRangeIter<'a> {
        BitSetRangeIter {
            set,
            page_info_index: 0,
            page_iter: BitSetRangeIter::<'a>::page_iter(set, 0),
        }
    }

    fn move_to_next_page(&mut self) -> bool {
        self.page_info_index += 1;
        self.reset_page_iter();
        self.page_iter.is_some()
    }

    fn reset_page_iter(&mut self) {
        self.page_iter = BitSetRangeIter::<'a>::page_iter(self.set, self.page_info_index);
    }

    fn page_iter(set: &'a BitSet, page_info_index: usize) -> Option<RangeIter<'a>> {
        set.page_map
            .get(page_info_index)
            .map(|pi| pi.index as usize)
            .and_then(|index| set.pages.get(index))
            .map(|p| p.iter_ranges())
    }

    fn next_range(&mut self) -> Option<RangeInclusive<u32>> {
        // TODO(garretrieger): don't recompute page start on each call.
        let page = self.set.page_map.get(self.page_info_index)?;
        let page_start = BitSet::major_start(page.major_value);
        self.page_iter
            .as_mut()?
            .next()
            .map(|r| (r.start() + page_start)..=(r.end() + page_start))
    }
}

impl<'a> Iterator for BitSetRangeIter<'a> {
    type Item = RangeInclusive<u32>;

    fn next(&mut self) -> Option<Self::Item> {
        self.page_iter.as_ref()?;
        let mut current_range = self.next_range();
        loop {
            let page = self.set.page_map.get(self.page_info_index)?;
            let page_end = BitSet::major_end(page.major_value);

            let Some(range) = current_range.clone() else {
                // The current page has no more ranges, but there may be more pages.
                if !self.move_to_next_page() {
                    return None;
                }
                current_range = self.next_range();
                continue;
            };

            if *range.end() != page_end {
                break;
            }

            // The range goes right to the end of the current page and may continue into it.
            self.move_to_next_page();
            let continuation = self.next_range();
            let Some(continuation) = continuation else {
                break;
            };

            if *continuation.start() == *range.end() + 1 {
                current_range = Some(*range.start()..=*continuation.end());
                continue;
            }

            // Continuation range does not touch the current range, ignore it and return what we have.
            // Since we consumed an item from the new page iterator, reset it.
            self.reset_page_iter();
            break;
        }

        current_range
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::collections::HashSet;

    impl FromIterator<u32> for BitSet {
        fn from_iter<I: IntoIterator<Item = u32>>(iter: I) -> Self {
            let mut out = BitSet::empty();
            out.extend(iter);
            out
        }
    }

    #[test]
    fn len() {
        let bitset = BitSet::empty();
        assert_eq!(bitset.len(), 0);
        assert!(bitset.is_empty());
    }

    #[test]
    fn iter() {
        let mut bitset = BitSet::empty();
        bitset.insert(3);
        bitset.insert(8);
        bitset.insert(534);
        bitset.insert(700);
        bitset.insert(10000);
        bitset.insert(10001);
        bitset.insert(10002);

        let v: Vec<u32> = bitset.iter().collect();
        assert_eq!(v, vec![3, 8, 534, 700, 10000, 10001, 10002]);
    }

    fn check_iter_ranges(ranges: Vec<RangeInclusive<u32>>) {
        let mut set = BitSet::empty();
        for range in ranges.iter() {
            set.insert_range(*range.start()..=*range.end());
        }
        let items: Vec<_> = set.iter_ranges().collect();
        assert_eq!(items, ranges);
    }

    #[test]
    fn iter_ranges() {
        check_iter_ranges(vec![0..=0]);
        check_iter_ranges(vec![4578..=4578]);
        check_iter_ranges(vec![0..=10, 4578..=4583]);
        check_iter_ranges(vec![0..=700]);
        check_iter_ranges(vec![353..=737]);

        check_iter_ranges(vec![u32::MAX..=u32::MAX]);
        check_iter_ranges(vec![(u32::MAX - 10)..=u32::MAX]);
        check_iter_ranges(vec![0..=5, (u32::MAX - 5)..=u32::MAX]);

        check_iter_ranges(vec![0..=511, 513..=517]);
        check_iter_ranges(vec![512..=1023, 1025..=1027]);

        check_iter_ranges(vec![1792..=2650]);
    }

    #[test]
    fn iter_ranges_zero_pages() {
        let mut set = BitSet::empty();

        set.insert(1000);
        set.insert_range(300..=511);
        set.remove(1000);

        let items: Vec<_> = set.iter_ranges().collect();
        assert_eq!(items, vec![300..=511]);
    }

    #[test]
    fn iter_backwards() {
        let mut bitset = BitSet::empty();

        bitset.insert_range(1..=6);
        {
            let mut it = bitset.iter();
            assert_eq!(Some(1), it.next());
            assert_eq!(Some(6), it.next_back());
            assert_eq!(Some(5), it.next_back());
            assert_eq!(Some(2), it.next());
            assert_eq!(Some(3), it.next());
            assert_eq!(Some(4), it.next());
            assert_eq!(None, it.next());
            assert_eq!(None, it.next_back());
        }

        bitset.insert_range(700..=701);
        {
            let mut it = bitset.iter();
            assert_eq!(Some(1), it.next());
            assert_eq!(Some(701), it.next_back());
            assert_eq!(Some(700), it.next_back());
            assert_eq!(Some(6), it.next_back());
            assert_eq!(Some(5), it.next_back());
            assert_eq!(Some(2), it.next());
            assert_eq!(Some(3), it.next());
            assert_eq!(Some(4), it.next());
            assert_eq!(None, it.next());
            assert_eq!(None, it.next_back());
        }

        let v: Vec<u32> = bitset.iter().rev().collect();
        assert_eq!(vec![701, 700, 6, 5, 4, 3, 2, 1], v);
    }

    #[test]
    fn iter_after() {
        let mut bitset = BitSet::empty();
        bitset.extend([5, 7, 10, 1250, 1300, 3001]);

        assert_eq!(
            bitset.iter_after(0).collect::<Vec<u32>>(),
            vec![5, 7, 10, 1250, 1300, 3001]
        );

        assert_eq!(
            bitset.iter_after(5).collect::<Vec<u32>>(),
            vec![7, 10, 1250, 1300, 3001]
        );
        assert_eq!(
            bitset.iter_after(6).collect::<Vec<u32>>(),
            vec![7, 10, 1250, 1300, 3001]
        );

        assert_eq!(
            bitset.iter_after(10).collect::<Vec<u32>>(),
            vec![1250, 1300, 3001]
        );

        assert_eq!(
            bitset.iter_after(700).collect::<Vec<u32>>(),
            vec![1250, 1300, 3001]
        );

        assert_eq!(
            bitset.iter_after(1250).collect::<Vec<u32>>(),
            vec![1300, 3001]
        );

        assert_eq!(bitset.iter_after(3000).collect::<Vec<u32>>(), vec![3001]);
        assert_eq!(bitset.iter_after(3001).collect::<Vec<u32>>(), vec![]);
        assert_eq!(bitset.iter_after(3002).collect::<Vec<u32>>(), vec![]);
        assert_eq!(bitset.iter_after(5000).collect::<Vec<u32>>(), vec![]);
        assert_eq!(bitset.iter_after(u32::MAX).collect::<Vec<u32>>(), vec![]);

        bitset.insert(u32::MAX);
        assert_eq!(bitset.iter_after(u32::MAX).collect::<Vec<u32>>(), vec![]);
        assert_eq!(
            bitset.iter_after(u32::MAX - 1).collect::<Vec<u32>>(),
            vec![u32::MAX]
        );

        let mut bitset = BitSet::empty();
        bitset.extend([510, 511, 512]);

        assert_eq!(bitset.iter_after(510).collect::<Vec<u32>>(), vec![511, 512]);
        assert_eq!(bitset.iter_after(511).collect::<Vec<u32>>(), vec![512]);
        assert_eq!(bitset.iter_after(512).collect::<Vec<u32>>(), vec![]);
    }

    #[test]
    fn extend() {
        let values = [3, 8, 534, 700, 10000, 10001, 10002];
        let values_unsorted = [10000, 3, 534, 700, 8, 10001, 10002];

        let mut s1 = BitSet::empty();
        let mut s2 = BitSet::empty();
        let mut s3 = BitSet::empty();
        let mut s4 = BitSet::empty();
        assert_eq!(s1.len(), 0);

        s1.extend(values.iter().copied());
        s2.extend_unsorted(values.iter().copied());
        s3.extend(values_unsorted.iter().copied());
        s4.extend_unsorted(values_unsorted.iter().copied());

        assert_eq!(s1.iter().collect::<Vec<u32>>(), values);
        assert_eq!(s2.iter().collect::<Vec<u32>>(), values);
        assert_eq!(s3.iter().collect::<Vec<u32>>(), values);
        assert_eq!(s4.iter().collect::<Vec<u32>>(), values);

        assert_eq!(s1.len(), 7);
        assert_eq!(s2.len(), 7);
        assert_eq!(s3.len(), 7);
        assert_eq!(s4.len(), 7);
    }

    #[test]
    fn insert_unordered() {
        let mut bitset = BitSet::empty();

        assert!(!bitset.contains(0));
        assert!(!bitset.contains(768));
        assert!(!bitset.contains(1678));

        assert!(bitset.insert(0));
        assert!(bitset.insert(1678));
        assert!(bitset.insert(768));

        assert!(bitset.contains(0));
        assert!(bitset.contains(768));
        assert!(bitset.contains(1678));

        assert!(!bitset.contains(1));
        assert!(!bitset.contains(769));
        assert!(!bitset.contains(1679));

        assert_eq!(bitset.len(), 3);
    }

    #[test]
    fn remove() {
        let mut bitset = BitSet::empty();

        assert!(bitset.insert(0));
        assert!(bitset.insert(511));
        assert!(bitset.insert(512));
        assert!(bitset.insert(1678));
        assert!(bitset.insert(768));

        assert_eq!(bitset.len(), 5);

        assert!(!bitset.remove(12));
        assert!(bitset.remove(511));
        assert!(bitset.remove(512));
        assert!(!bitset.remove(512));

        assert_eq!(bitset.len(), 3);
        assert!(bitset.contains(0));
        assert!(!bitset.contains(511));
        assert!(!bitset.contains(512));
    }

    #[test]
    fn remove_all() {
        let mut bitset = BitSet::empty();
        bitset.extend([5, 7, 11, 18, 620, 2000]);

        assert_eq!(bitset.len(), 6);

        bitset.remove_all([7, 11, 13, 18, 620]);
        assert_eq!(bitset.len(), 2);
        assert_eq!(bitset.iter().collect::<Vec<u32>>(), vec![5, 2000]);
    }

    #[test]
    fn remove_range() {
        let mut bitset = BitSet::empty();
        bitset.extend([5, 7, 11, 18, 511, 620, 1023, 1024, 1200]);
        assert_eq!(bitset.len(), 9);
        bitset.remove_range(7..=620);
        assert_eq!(bitset.len(), 4);
        assert_eq!(
            bitset.iter().collect::<Vec<u32>>(),
            vec![5, 1023, 1024, 1200]
        );

        let mut bitset = BitSet::empty();
        bitset.extend([5, 7, 11, 18, 511, 620, 1023, 1024, 1200]);
        bitset.remove_range(7..=1024);
        assert_eq!(bitset.len(), 2);
        assert_eq!(bitset.iter().collect::<Vec<u32>>(), vec![5, 1200]);

        let mut bitset = BitSet::empty();
        bitset.extend([5, 7, 11, 18, 511, 620, 1023, 1024, 1200]);
        bitset.remove_range(2000..=2100);
        assert_eq!(bitset.len(), 9);
        assert_eq!(
            bitset.iter().collect::<Vec<u32>>(),
            vec![5, 7, 11, 18, 511, 620, 1023, 1024, 1200]
        );

        // Remove all from one page
        let mut bitset = BitSet::empty();
        bitset.extend([1001, 1002, 1003, 1004]);
        bitset.remove_range(1002..=1003);
        assert!(bitset.contains(1001));
        assert!(!bitset.contains(1002));
        assert!(!bitset.contains(1003));
        assert!(bitset.contains(1004));

        bitset.remove_range(100..=200);
        assert!(bitset.contains(1001));
        assert!(!bitset.contains(1002));
        assert!(!bitset.contains(1003));
        assert!(bitset.contains(1004));

        bitset.remove_range(100..=1001);
        assert!(!bitset.contains(1001));
        assert!(!bitset.contains(1002));
        assert!(!bitset.contains(1003));
        assert!(bitset.contains(1004));
    }

    #[test]
    fn remove_range_boundary() {
        let mut set = BitSet::empty();

        set.remove_range(u32::MAX - 10..=u32::MAX);
        assert!(!set.contains(u32::MAX));
        set.insert_range(u32::MAX - 10..=u32::MAX);
        assert!(set.contains(u32::MAX));
        set.remove_range(u32::MAX - 10..=u32::MAX);
        assert!(!set.contains(u32::MAX));

        set.remove_range(0..=10);
        assert!(!set.contains(0));
        set.insert_range(0..=10);
        assert!(set.contains(0));
        set.remove_range(0..=10);
        assert!(!set.contains(0));
    }

    #[test]
    fn remove_to_empty_page() {
        let mut bitset = BitSet::empty();

        bitset.insert(793);
        bitset.insert(43);
        bitset.remove(793);

        assert!(bitset.contains(43));
        assert!(!bitset.contains(793));
        assert_eq!(bitset.len(), 1);
    }

    #[test]
    fn insert_max_value() {
        let mut bitset = BitSet::empty();
        assert!(!bitset.contains(u32::MAX));
        assert!(bitset.insert(u32::MAX));
        assert!(bitset.contains(u32::MAX));
        assert!(!bitset.contains(u32::MAX - 1));
        assert_eq!(bitset.len(), 1);
    }

    fn check_process<A, B, C, Op>(a: A, b: B, expected: C, op: Op)
    where
        A: IntoIterator<Item = u32>,
        B: IntoIterator<Item = u32>,
        C: IntoIterator<Item = u32>,
        Op: Fn(&mut BitSet, &BitSet),
    {
        let mut result = BitSet::from_iter(a);
        let b_set = BitSet::from_iter(b);
        let expected_set = BitSet::from_iter(expected);
        result.len();

        op(&mut result, &b_set);
        assert_eq!(result, expected_set);
        assert_eq!(result.len(), expected_set.len());
    }

    #[test]
    fn union() {
        check_process([], [5], [5], |a, b| a.union(b));
        check_process([128], [5], [128, 5], |a, b| a.union(b));
        check_process([128], [], [128], |a, b| a.union(b));
        check_process([1280], [5], [5, 1280], |a, b| a.union(b));
        check_process([5], [1280], [5, 1280], |a, b| a.union(b));
    }

    #[test]
    fn intersect() {
        check_process([], [5], [], |a, b| a.intersect(b));
        check_process([5], [], [], |a, b| a.intersect(b));
        check_process([1, 5, 9], [5, 7], [5], |a, b| a.intersect(b));
        check_process([1, 1000, 2000], [1000], [1000], |a, b| a.intersect(b));
        check_process([1000], [1, 1000, 2000], [1000], |a, b| a.intersect(b));
        check_process([1, 1000, 2000], [1000, 5000], [1000], |a, b| a.intersect(b));
    }

    #[test]
    fn subtract() {
        check_process([], [5], [], |a, b| a.subtract(b));
        check_process([5], [], [5], |a, b| a.subtract(b));
        check_process([5, 1000], [1000], [5], |a, b| a.subtract(b));
        check_process([5, 1000], [5], [1000], |a, b| a.subtract(b));
    }

    #[test]
    fn reversed_subtract() {
        check_process([], [5], [5], |a, b| a.reversed_subtract(b));
        check_process([5], [], [], |a, b| a.reversed_subtract(b));
        check_process([1000], [5, 1000], [5], |a, b| a.reversed_subtract(b));
        check_process([5], [5, 1000], [1000], |a, b| a.reversed_subtract(b));
    }

    fn set_for_range(first: u32, last: u32) -> BitSet {
        let mut set = BitSet::empty();
        for i in first..=last {
            set.insert(i);
        }
        set
    }

    #[test]
    fn insert_range() {
        for range in [
            (0, 0),
            (0, 364),
            (0, 511),
            (512, 1023),
            (0, 1023),
            (364, 700),
            (364, 6000),
        ] {
            let mut set = BitSet::empty();
            set.len();
            set.insert_range(range.0..=range.1);
            assert_eq!(set, set_for_range(range.0, range.1), "{range:?}");
            assert_eq!(set.len(), (range.1 - range.0 + 1) as u64, "{range:?}");
        }
    }

    #[test]
    fn insert_range_on_existing() {
        let mut set = BitSet::empty();
        set.insert(700);
        set.insert(2000);
        set.insert_range(32..=4000);
        assert_eq!(set, set_for_range(32, 4000));
        assert_eq!(set.len(), 4000 - 32 + 1);
    }

    #[test]
    fn insert_range_max() {
        let mut set = BitSet::empty();
        set.insert_range(u32::MAX..=u32::MAX);
        assert!(set.contains(u32::MAX));
        assert_eq!(set.len(), 1);
    }

    #[test]
    fn clear() {
        let mut bitset = BitSet::empty();

        bitset.insert(13);
        bitset.insert(670);
        assert!(bitset.contains(13));
        assert!(bitset.contains(670));

        bitset.clear();
        assert!(!bitset.contains(13));
        assert!(!bitset.contains(670));
        assert_eq!(bitset.len(), 0);
    }

    #[test]
    #[allow(clippy::mutable_key_type)]
    fn hash_and_eq() {
        let mut bitset1 = BitSet::empty();
        let mut bitset2 = BitSet::empty();
        let mut bitset3 = BitSet::empty();
        let mut bitset4 = BitSet::empty();

        bitset1.insert(43);
        bitset1.insert(793);

        bitset2.insert(793);
        bitset2.insert(43);
        bitset2.len();

        bitset3.insert(43);
        bitset3.insert(793);
        bitset3.insert(794);

        bitset4.insert(0);

        assert_eq!(BitSet::empty(), BitSet::empty());
        assert_eq!(bitset1, bitset2);
        assert_ne!(bitset1, bitset3);
        assert_ne!(bitset2, bitset3);
        assert_eq!(bitset4, bitset4);

        let set = HashSet::from([bitset1]);
        assert!(set.contains(&bitset2));
        assert!(!set.contains(&bitset3));
    }

    #[test]
    #[allow(clippy::mutable_key_type)]
    fn hash_and_eq_with_empty_pages() {
        let mut bitset1 = BitSet::empty();
        let mut bitset2 = BitSet::empty();
        let mut bitset3 = BitSet::empty();

        bitset1.insert(43);

        bitset2.insert(793);
        bitset2.insert(43);
        bitset2.remove(793);

        bitset3.insert(43);
        bitset3.insert(793);

        assert_eq!(bitset1, bitset2);
        assert_ne!(bitset1, bitset3);

        let set = HashSet::from([bitset1]);
        assert!(set.contains(&bitset2));
    }

    #[test]
    fn ordering() {
        macro_rules! assert_ord {
            ($lhs:expr, $rhs:expr, $ord:path) => {
                assert_eq!(
                    BitSet::from_iter($lhs).cmp(&BitSet::from_iter($rhs)),
                    $ord,
                    "{:?}, {:?}",
                    $lhs,
                    $rhs
                )
            };
        }

        const EMPTY: [u32; 0] = [];
        assert_ord!(EMPTY, EMPTY, Ordering::Equal);
        assert_ord!(EMPTY, [0], Ordering::Less);
        assert_ord!([0], [0], Ordering::Equal);
        assert_ord!([0, 1, 2], [1, 2, 3], Ordering::Less);
        assert_ord!([0, 1, 4], [1, 2, 3], Ordering::Less);
        assert_ord!([1, 2, 3], [0, 2, 4], Ordering::Greater);
        assert_ord!([5, 4, 0], [1, 2, 3], Ordering::Less); // out of order
        assert_ord!([1, 2, 3], [1, 2, 3, 4], Ordering::Less); // out of order
        assert_ord!([2, 3, 4], [1, 2, 3, 4, 5], Ordering::Greater); // out of order

        assert_ord!([1000, 2000, 3000], [1000, 2000, 3000, 4000], Ordering::Less); // out of order
        assert_ord!([1000, 1001,], [1000, 2000], Ordering::Less); // out of order
        assert_ord!(
            [2000, 3000, 4000],
            [1000, 2000, 3000, 4000, 5000],
            Ordering::Greater
        ); // out of order
    }
}
