//! Stores a page of bits, used inside of bitset's.

use std::{cell::Cell, hash::Hash, ops::RangeInclusive};

// the integer type underlying our bit set
type Element = u64;

// the number of elements in a page
const PAGE_SIZE: u32 = 8;
// the length of an element in bytes
const ELEM_SIZE: u32 = std::mem::size_of::<Element>() as u32;
// the length of an element in bits
const ELEM_BITS: u32 = ELEM_SIZE * 8;
// mask out bits of a value not used to index into an element
const ELEM_MASK: u32 = ELEM_BITS - 1;
// the number of bits in a page
pub(crate) const PAGE_BITS: u32 = ELEM_BITS * PAGE_SIZE;
// mask out the bits of a value not used to index into a page
const PAGE_MASK: u32 = PAGE_BITS - 1;

/// A fixed size (512 bits wide) page of bits that records integer set membership from `[0, 511]`.
#[derive(Clone)]
pub(crate) struct BitPage {
    storage: [Element; PAGE_SIZE as usize],
    len: Cell<u32>,
}

impl BitPage {
    /// Create a new page with no bits set.
    pub(crate) fn new_zeroes() -> Self {
        Self {
            storage: [0; PAGE_SIZE as usize],
            len: Cell::new(0),
        }
    }

    /// Returns the number of members in this page.
    pub(crate) fn len(&self) -> u32 {
        if self.is_dirty() {
            // this means we're stale and should recompute
            let len = self.storage.iter().map(|val| val.count_ones()).sum();
            self.len.set(len);
        }
        self.len.get()
    }

    /// Returns true if this page has no members.
    pub(crate) fn is_empty(&self) -> bool {
        self.len() == 0
    }

    // TODO(garretrieger): iterator that starts after some value (similar to next in hb).
    // TODO(garretrieger): reverse iterator.

    /// Iterator over the members of this page.
    pub(crate) fn iter(&self) -> impl DoubleEndedIterator<Item = u32> + '_ {
        self.storage
            .iter()
            .enumerate()
            .filter(|(_, elem)| **elem != 0)
            .flat_map(|(i, elem)| {
                let base = i as u32 * ELEM_BITS;
                Iter::new(*elem).map(move |idx| base + idx)
            })
    }

    /// Iterator over the members of this page that come after `value`.
    pub(crate) fn iter_after(&self, value: u32) -> impl DoubleEndedIterator<Item = u32> + '_ {
        let start_index = Self::element_index(value);
        self.storage[start_index..]
            .iter()
            .enumerate()
            .filter(|(_, elem)| **elem != 0)
            .flat_map(move |(i, elem)| {
                let i = i + start_index;
                let base = i as u32 * ELEM_BITS;
                let index_in_elem = value & ELEM_MASK;
                let it = if start_index == i {
                    Iter::from(*elem, index_in_elem + 1)
                } else {
                    Iter::new(*elem)
                };
                it.map(move |idx| base + idx)
            })
    }

    /// Iterator over the ranges in this page.
    pub(crate) fn iter_ranges(&self) -> RangeIter<'_> {
        RangeIter {
            page: self,
            next_value_to_check: 0,
        }
    }

    /// Marks `(val % page width)` a member of this set and returns `true` if it is newly added.
    pub(crate) fn insert(&mut self, val: u32) -> bool {
        let ret = !self.contains(val);
        *self.element_mut(val) |= elem_index_bit_mask(val);
        self.mark_dirty();
        ret
    }

    /// Marks `(val % page width)` a member of this set, but does not check if it was already a member.
    ///
    /// This is used to maximize performance in cases where the return value on [`insert()`] is not needed.
    ///
    /// [`insert()`]: Self::insert
    pub(crate) fn insert_no_return(&mut self, val: u32) {
        *self.element_mut(val) |= elem_index_bit_mask(val);
        self.mark_dirty();
    }

    /// Marks all values `[first, last]` as members of this set.
    pub(crate) fn insert_range(&mut self, first: u32, last: u32) {
        let first = first & PAGE_MASK;
        let last = last & PAGE_MASK;
        let first_elem_idx = first / ELEM_BITS;
        let last_elem_idx = last / ELEM_BITS;

        for elem_idx in first_elem_idx..=last_elem_idx {
            let elem_start = first.max(elem_idx * ELEM_BITS) & ELEM_MASK;
            let elem_last = last.min(((elem_idx + 1) * ELEM_BITS) - 1) & ELEM_MASK;

            let end_shift = ELEM_BITS - elem_last - 1;
            let mask = u64::MAX << (elem_start + end_shift);
            let mask = mask >> end_shift;

            self.storage[elem_idx as usize] |= mask;
        }

        self.mark_dirty();
    }

    /// Marks all values `[first, last]` as not members of this set.
    pub(crate) fn remove_range(&mut self, first: u32, last: u32) {
        let first = first & PAGE_MASK;
        let last = last & PAGE_MASK;
        let first_elem_idx = first / ELEM_BITS;
        let last_elem_idx = last / ELEM_BITS;

        for elem_idx in first_elem_idx..=last_elem_idx {
            let elem_start = first.max(elem_idx * ELEM_BITS) & ELEM_MASK;
            let elem_last = last.min(((elem_idx + 1) * ELEM_BITS) - 1) & ELEM_MASK;

            let end_shift = ELEM_BITS - elem_last - 1;
            let mask = u64::MAX << (elem_start + end_shift);
            let mask = !(mask >> end_shift);

            self.storage[elem_idx as usize] &= mask;
        }

        self.mark_dirty();
    }

    pub(crate) fn clear(&mut self) {
        for elem in self.storage.iter_mut() {
            *elem = 0;
        }
        self.len.set(0);
    }

    /// Removes `(val % page width)` from this set.
    pub(crate) fn remove(&mut self, val: u32) -> bool {
        let ret = self.contains(val);
        *self.element_mut(val) &= !elem_index_bit_mask(val);
        self.mark_dirty();
        ret
    }

    /// Return true if `(val % page width)` is a member of this set.
    pub(crate) fn contains(&self, val: u32) -> bool {
        (*self.element(val) & elem_index_bit_mask(val)) != 0
    }

    pub(crate) fn union(a: &BitPage, b: &BitPage) -> BitPage {
        a.process(b, |a, b| a | b)
    }

    pub(crate) fn intersect(a: &BitPage, b: &BitPage) -> BitPage {
        a.process(b, |a, b| a & b)
    }

    pub(crate) fn subtract(a: &BitPage, b: &BitPage) -> BitPage {
        a.process(b, |a, b| a & !b)
    }

    fn process<Op>(&self, other: &BitPage, op: Op) -> BitPage
    where
        Op: Fn(Element, Element) -> Element,
    {
        let mut out = BitPage::new_zeroes();
        out.mark_dirty();
        for i in 0usize..(PAGE_SIZE as usize) {
            out.storage[i] = op(self.storage[i], other.storage[i]);
        }
        out
    }

    fn mark_dirty(&mut self) {
        self.len.set(u32::MAX);
    }

    fn is_dirty(&self) -> bool {
        self.len.get() == u32::MAX
    }

    fn element(&self, value: u32) -> &Element {
        &self.storage[Self::element_index(value)]
    }

    fn element_mut(&mut self, value: u32) -> &mut Element {
        &mut self.storage[Self::element_index(value)]
    }

    const fn element_index(value: u32) -> usize {
        (value as usize & PAGE_MASK as usize) / (ELEM_BITS as usize)
    }
}

/// returns the bit to set in an element for this value
const fn elem_index_bit_mask(value: u32) -> Element {
    1 << (value & ELEM_MASK)
}

struct Iter {
    val: Element,
    forward_index: i32,
    backward_index: i32,
}

impl Iter {
    fn new(elem: Element) -> Iter {
        Iter {
            val: elem,
            forward_index: 0,
            backward_index: ELEM_BITS as i32 - 1,
        }
    }

    /// Construct an iterator that starts at `index`
    ///
    /// Specifically if `index` bit is set it will be returned on the first call to `next()`.
    fn from(elem: Element, index: u32) -> Iter {
        Iter {
            val: elem,
            forward_index: index as i32, // index is at most 63
            backward_index: ELEM_BITS as i32 - 1,
        }
    }
}

impl Iterator for Iter {
    type Item = u32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.forward_index > self.backward_index {
            return None;
        }
        let mask = (1u64 << self.forward_index) - 1;
        let masked = self.val & !mask;
        let next_index = masked.trailing_zeros() as i32;
        if next_index > self.backward_index {
            return None;
        }
        self.forward_index = next_index + 1;
        Some(next_index as u32)
    }
}

impl DoubleEndedIterator for Iter {
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.backward_index < self.forward_index {
            return None;
        }

        let mask = 1u64
            .checked_shl(self.backward_index as u32 + 1)
            .map(|v| v - 1)
            .unwrap_or(Element::MAX);
        let masked = self.val & mask;
        let next_index = (ELEM_BITS as i32) - (masked.leading_zeros() as i32) - 1;
        if next_index < self.forward_index {
            return None;
        }
        self.backward_index = next_index - 1;
        Some(next_index as u32)
    }
}

pub(crate) struct RangeIter<'a> {
    page: &'a BitPage,
    next_value_to_check: u32,
}

impl<'a> RangeIter<'a> {
    fn next_range_in_element(&self) -> Option<RangeInclusive<u32>> {
        if self.next_value_to_check >= PAGE_BITS {
            return None;
        }

        let element = self.page.element(self.next_value_to_check);
        let element_bit = (self.next_value_to_check & ELEM_MASK) as u64;
        let major = self.next_value_to_check & !ELEM_MASK;

        let mask = !((1 << element_bit) - 1);
        let range_start = (element & mask).trailing_zeros();
        if range_start == ELEM_BITS {
            // There's no remaining values in this element.
            return None;
        }

        let mask = (1 << range_start) - 1;
        let range_end = (element | mask).trailing_ones() - 1;

        Some((major + range_start)..=(major + range_end))
    }
}

impl<'a> Iterator for RangeIter<'a> {
    type Item = RangeInclusive<u32>;

    fn next(&mut self) -> Option<Self::Item> {
        let mut current_range = self.next_range_in_element();
        loop {
            let element_end = (self.next_value_to_check & !ELEM_MASK) + ELEM_BITS - 1;
            let Some(range) = current_range.clone() else {
                // No more ranges in the current element, move to the next one.
                self.next_value_to_check = element_end + 1;
                if self.next_value_to_check < PAGE_BITS {
                    current_range = self.next_range_in_element();
                    continue;
                } else {
                    return None;
                }
            };

            self.next_value_to_check = range.end() + 1;
            if *range.end() == element_end {
                let continuation = self.next_range_in_element();
                if let Some(continuation) = continuation {
                    if *continuation.start() == element_end + 1 {
                        current_range = Some(*range.start()..=*continuation.end());
                        continue;
                    }
                }
            }

            break;
        }

        current_range
    }
}

impl Default for BitPage {
    fn default() -> Self {
        Self::new_zeroes()
    }
}

impl std::fmt::Debug for BitPage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        let values: Vec<_> = self.iter().collect();
        std::fmt::Debug::fmt(&values, f)
    }
}

impl Hash for BitPage {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.storage.hash(state);
    }
}

impl std::cmp::PartialEq for BitPage {
    fn eq(&self, other: &Self) -> bool {
        self.storage == other.storage
    }
}

impl std::cmp::Eq for BitPage {}

#[cfg(test)]
mod test {
    use std::collections::HashSet;

    use super::*;

    impl BitPage {
        /// Create a new page with all bits set.
        fn new_ones() -> Self {
            Self {
                storage: [Element::MAX; PAGE_SIZE as usize],
                len: Cell::new(PAGE_SIZE * ELEM_BITS),
            }
        }
    }

    impl FromIterator<u32> for BitPage {
        fn from_iter<I: IntoIterator<Item = u32>>(iter: I) -> Self {
            let mut out = BitPage::new_zeroes();
            for v in iter {
                out.insert(v);
            }
            out
        }
    }

    #[test]
    fn test_iter_bit_indices() {
        let items: Vec<_> = Iter::new(0).collect();
        assert_eq!(items, vec![]);

        let items: Vec<_> = Iter::new(1).collect();
        assert_eq!(items, vec![0]);

        let items: Vec<_> = Iter::new(0b1100).collect();
        assert_eq!(items, vec![2, 3]);

        let items: Vec<_> = Iter::new(1 << 63).collect();
        assert_eq!(items, vec![63]);

        let items: Vec<_> = Iter::new((1 << 47) | (1 << 63)).collect();
        assert_eq!(items, vec![47, 63]);

        assert_eq!(Iter::new(Element::MAX).max(), Some(ELEM_BITS - 1));
        assert_eq!(Iter::new(Element::MAX).min(), Some(0));
    }

    #[test]
    fn test_iter_bit_indices_backwards() {
        let mut it = Iter::new(0);
        assert_eq!(None, it.next());
        assert_eq!(None, it.next_back());

        let mut it = Iter::new((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
        assert_eq!(Some(1), it.next());
        assert_eq!(Some(6), it.next_back());
        assert_eq!(Some(5), it.next_back());
        assert_eq!(Some(2), it.next());
        assert_eq!(Some(3), it.next());
        assert_eq!(Some(4), it.next());
        assert_eq!(None, it.next());
        assert_eq!(None, it.next_back());

        let mut it = Iter::new(1);
        assert_eq!(Some(0), it.next_back());
        assert_eq!(None, it.next_back());

        let mut it = Iter::new(1 << 63);
        assert_eq!(Some(63), it.next_back());
        assert_eq!(None, it.next_back());

        let mut it = Iter::new((1 << 63) | (1 << 62));
        assert_eq!(Some(63), it.next_back());
        assert_eq!(Some(62), it.next_back());
        assert_eq!(None, it.next_back());

        let mut it = Iter::new((1 << 63) | (1 << 32));
        assert_eq!(Some(63), it.next_back());
        assert_eq!(Some(32), it.next_back());
        assert_eq!(None, it.next_back());
    }

    #[test]
    fn page_init() {
        let page = BitPage::new_zeroes();
        assert_eq!(page.len(), 0);
        assert!(page.is_empty());
    }

    #[test]
    fn page_init_ones() {
        let page = BitPage::new_ones();
        assert_eq!(page.len(), 512);
        assert!(!page.is_empty());
    }

    #[test]
    fn page_contains_empty() {
        let page = BitPage::new_zeroes();
        assert!(!page.contains(0));
        assert!(!page.contains(1));
        assert!(!page.contains(75475));
    }

    #[test]
    fn page_contains_all() {
        let page = BitPage::new_ones();
        assert!(page.contains(0));
        assert!(page.contains(1));
        assert!(page.contains(75475));
    }

    #[test]
    fn page_insert() {
        for val in 0..=1025 {
            let mut page = BitPage::new_zeroes();
            assert!(!page.contains(val), "unexpected {val} (1)");
            page.insert(val);
            assert!(page.contains(val), "missing {val}");
            assert!(!page.contains(val.wrapping_sub(1)), "unexpected {val} (2)");
        }
    }

    #[test]
    fn page_insert_range() {
        fn page_for_range(first: u32, last: u32) -> BitPage {
            let mut page = BitPage::new_zeroes();
            for i in first..=last {
                page.insert(i);
            }
            page
        }

        for range in [
            (0, 0),
            (0, 1),
            (1, 15),
            (5, 63),
            (64, 67),
            (69, 72),
            (69, 127),
            (32, 345),
            (512 + 32, 512 + 345),
            (0, 511),
        ] {
            let mut page = BitPage::new_zeroes();
            page.insert_range(range.0, range.1);
            assert_eq!(page, page_for_range(range.0, range.1), "{range:?}");
        }
    }

    #[test]
    fn page_insert_return() {
        let mut page = BitPage::new_zeroes();
        assert!(page.insert(123));
        assert!(!page.insert(123));
    }

    #[test]
    fn page_remove() {
        for val in 0..=1025 {
            let mut page = BitPage::new_ones();
            assert!(page.contains(val), "missing {val} (1)");
            assert!(page.remove(val));
            assert!(!page.remove(val));
            assert!(!page.contains(val), "unexpected {val}");
            assert!(page.contains(val.wrapping_sub(1)), "missing {val} (2)");
        }
    }

    #[test]
    fn page_remove_range() {
        fn page_for_range(first: u32, last: u32) -> BitPage {
            let mut page = BitPage::new_ones();
            for i in first..=last {
                page.remove(i);
            }
            page
        }

        for exclude_range in [
            (0, 0),
            (0, 1),
            (1, 15),
            (5, 63),
            (64, 67),
            (69, 72),
            (69, 127),
            (32, 345),
            (0, 511),
            (512 + 32, 512 + 345),
        ] {
            let mut page = BitPage::new_ones();
            page.remove_range(exclude_range.0, exclude_range.1);
            assert_eq!(
                page,
                page_for_range(exclude_range.0, exclude_range.1),
                "{exclude_range:?}"
            );
        }
    }

    #[test]
    fn clear() {
        let mut zeroes = BitPage::new_zeroes();
        let mut ones = BitPage::new_ones();

        zeroes.clear();
        assert_eq!(zeroes.len(), 0);
        assert_eq!(zeroes.iter().next(), None);

        zeroes.insert_range(10, 300);
        zeroes.clear();
        assert_eq!(zeroes.len(), 0);
        assert_eq!(zeroes.iter().next(), None);

        ones.clear();
        assert_eq!(ones.len(), 0);
        assert_eq!(ones.iter().next(), None);
    }

    #[test]
    fn remove_to_empty_page() {
        let mut page = BitPage::new_zeroes();

        page.insert(13);
        assert!(!page.is_empty());

        page.remove(13);
        assert!(page.is_empty());
    }

    #[test]
    fn page_iter() {
        let mut page = BitPage::new_zeroes();

        page.insert(0);
        page.insert(12);
        page.insert(13);
        page.insert(63);
        page.insert(64);
        page.insert(511);
        page.insert(23);
        page.insert(400);
        page.insert(78);

        let items: Vec<_> = page.iter().collect();
        assert_eq!(items, vec![0, 12, 13, 23, 63, 64, 78, 400, 511,])
    }

    #[test]
    fn page_iter_overflow() {
        let mut page = BitPage::new_zeroes();
        page.insert(0);
        let mut it = page.iter();
        assert_eq!(Some(0), it.next_back());
        assert_eq!(None, it.next());
    }

    #[test]
    fn page_iter_after() {
        let mut page = BitPage::new_zeroes();
        let items: Vec<_> = page.iter_after(0).collect();
        assert_eq!(items, vec![]);
        let items: Vec<_> = page.iter_after(256).collect();
        assert_eq!(items, vec![]);

        page.insert(1);
        page.insert(12);
        page.insert(13);
        page.insert(63);
        page.insert(64);
        page.insert(511);
        page.insert(23);
        page.insert(400);
        page.insert(78);

        let items: Vec<_> = page.iter_after(0).collect();
        assert_eq!(items, vec![1, 12, 13, 23, 63, 64, 78, 400, 511,]);

        page.insert(0);
        let items: Vec<_> = page.iter_after(0).collect();
        assert_eq!(items, vec![1, 12, 13, 23, 63, 64, 78, 400, 511,]);

        let items: Vec<_> = page.iter_after(1).collect();
        assert_eq!(items, vec![12, 13, 23, 63, 64, 78, 400, 511,]);

        let items: Vec<_> = page.iter_after(63).collect();
        assert_eq!(items, vec![64, 78, 400, 511,]);

        let items: Vec<_> = page.iter_after(256).collect();
        assert_eq!(items, vec![400, 511]);

        let items: Vec<_> = page.iter_after(511).collect();
        assert_eq!(items, vec![]);

        let items: Vec<_> = page.iter_after(390).collect();
        assert_eq!(items, vec![400, 511]);

        let items: Vec<_> = page.iter_after(400).collect();
        assert_eq!(items, vec![511]);
    }

    #[test]
    fn page_iter_after_rev() {
        let mut page = BitPage::new_zeroes();
        let items: Vec<_> = page.iter_after(0).collect();
        assert_eq!(items, vec![]);
        let items: Vec<_> = page.iter_after(256).collect();
        assert_eq!(items, vec![]);

        page.insert(1);
        page.insert(12);
        page.insert(13);
        page.insert(63);
        page.insert(64);
        page.insert(511);
        page.insert(23);
        page.insert(400);
        page.insert(78);

        let items: Vec<_> = page.iter_after(0).rev().collect();
        assert_eq!(items, vec![511, 400, 78, 64, 63, 23, 13, 12, 1]);

        page.insert(0);
        let items: Vec<_> = page.iter_after(0).rev().collect();
        assert_eq!(items, vec![511, 400, 78, 64, 63, 23, 13, 12, 1]);

        let items: Vec<_> = page.iter_after(1).rev().collect();
        assert_eq!(items, vec![511, 400, 78, 64, 63, 23, 13, 12,]);

        let items: Vec<_> = page.iter_after(63).rev().collect();
        assert_eq!(items, vec![511, 400, 78, 64,]);

        let items: Vec<_> = page.iter_after(256).rev().collect();
        assert_eq!(items, vec![511, 400]);

        let items: Vec<_> = page.iter_after(511).rev().collect();
        assert_eq!(items, vec![]);

        let items: Vec<_> = page.iter_after(390).rev().collect();
        assert_eq!(items, vec![511, 400]);

        let items: Vec<_> = page.iter_after(400).rev().collect();
        assert_eq!(items, vec![511]);
    }

    fn check_iter_ranges(ranges: Vec<RangeInclusive<u32>>) {
        let mut page = BitPage::new_zeroes();
        for range in ranges.iter() {
            page.insert_range(*range.start(), *range.end());
        }
        let items: Vec<_> = page.iter_ranges().collect();
        assert_eq!(items, ranges);
    }

    #[test]
    fn iter_ranges() {
        // basic
        check_iter_ranges(vec![]);
        check_iter_ranges(vec![0..=5]);
        check_iter_ranges(vec![0..=0, 5..=5, 10..=10]);
        check_iter_ranges(vec![0..=5, 12..=31]);
        check_iter_ranges(vec![12..=31]);
        check_iter_ranges(vec![71..=84]);
        check_iter_ranges(vec![273..=284]);
        check_iter_ranges(vec![0..=511]);

        // end of boundary
        check_iter_ranges(vec![511..=511]);
        check_iter_ranges(vec![500..=511]);
        check_iter_ranges(vec![400..=511]);
        check_iter_ranges(vec![0..=511]);

        // continuation ranges
        check_iter_ranges(vec![64..=127]);
        check_iter_ranges(vec![64..=127, 129..=135]);
        check_iter_ranges(vec![64..=135]);
        check_iter_ranges(vec![71..=135]);
        check_iter_ranges(vec![71..=435]);
    }

    #[test]
    fn union() {
        let a = BitPage::new_zeroes();
        let b = BitPage::from_iter([32, 400]);
        let c = BitPage::from_iter([32, 200]);
        let d = BitPage::from_iter([32, 200, 400]);

        assert_eq!(BitPage::union(&a, &b), b);
        assert_eq!(BitPage::union(&b, &a), b);
        assert_eq!(BitPage::union(&b, &c), d);
        assert_eq!(BitPage::union(&c, &b), d);
    }

    #[test]
    fn intersect() {
        let a = BitPage::new_zeroes();
        let b = BitPage::from_iter([32, 400]);
        let c = BitPage::from_iter([32, 200]);
        let d = BitPage::from_iter([32]);

        assert_eq!(BitPage::intersect(&a, &b), a);
        assert_eq!(BitPage::intersect(&b, &a), a);
        assert_eq!(BitPage::intersect(&b, &c), d);
        assert_eq!(BitPage::intersect(&c, &b), d);
    }

    #[test]
    fn subtract() {
        let a = BitPage::new_zeroes();
        let b = BitPage::from_iter([32, 400]);
        let c = BitPage::from_iter([32, 200]);
        let d = BitPage::from_iter([400]);
        let e = BitPage::from_iter([200]);

        assert_eq!(BitPage::subtract(&a, &b), a);
        assert_eq!(BitPage::subtract(&b, &a), b);
        assert_eq!(BitPage::subtract(&b, &c), d);
        assert_eq!(BitPage::subtract(&c, &b), e);
    }

    #[test]
    #[allow(clippy::mutable_key_type)]
    fn hash_and_eq() {
        let mut page1 = BitPage::new_zeroes();
        let mut page2 = BitPage::new_zeroes();
        let mut page3 = BitPage::new_zeroes();

        page1.insert(12);
        page1.insert(300);

        page2.insert(300);
        page2.insert(12);
        page2.len();

        page3.insert(300);
        page3.insert(12);
        page3.insert(23);

        assert_eq!(page1, page2);
        assert_ne!(page1, page3);
        assert_ne!(page2, page3);

        let set = HashSet::from([page1]);
        assert!(set.contains(&page2));
        assert!(!set.contains(&page3));
    }
}
