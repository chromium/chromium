//! Various helpers for dealing with iteration of a slice
//! that represents a loop. Specifically, outline contours.

#[derive(Copy, Clone, Debug)]
pub(super) struct IndexCycler {
    last: usize,
}

impl IndexCycler {
    /// Creates a new index cycler for a collection of the given length.
    ///
    /// Returns `None` if the length is 0.
    pub fn new(len: usize) -> Option<Self> {
        Some(Self {
            last: len.checked_sub(1)?,
        })
    }

    pub fn next(self, ix: usize) -> usize {
        if ix >= self.last {
            0
        } else {
            ix + 1
        }
    }

    pub fn prev(self, ix: usize) -> usize {
        if ix == 0 {
            self.last
        } else {
            ix - 1
        }
    }
}

/// Iterator that begins at `start + 1` and cycles through all items
/// of the slice in forward order, ending with `start`.
pub(super) fn cycle_forward<T>(items: &[T], start: usize) -> impl Iterator<Item = (usize, &T)> {
    let len = items.len();
    let start = start + 1;
    (0..len).map(move |ix| {
        let real_ix = (ix + start) % len;
        (real_ix, &items[real_ix])
    })
}

/// Iterator that begins at `start - 1` and cycles through all items
/// of the slice in reverse order, ending with `start`.
pub(super) fn cycle_backward<T>(items: &[T], start: usize) -> impl Iterator<Item = (usize, &T)> {
    let len = items.len();
    (0..len).rev().map(move |ix| {
        let real_ix = (ix + start) % len;
        (real_ix, &items[real_ix])
    })
}

#[cfg(test)]
mod tests {
    use super::IndexCycler;

    #[test]
    fn cycler() {
        let cycler = IndexCycler::new(4).unwrap();
        // basic ops
        assert_eq!(cycler.next(0), 1);
        assert_eq!(cycler.prev(2), 1);
        // cycling ops
        assert_eq!(cycler.next(3), 0);
        assert_eq!(cycler.prev(0), 3);
        assert!(IndexCycler::new(0).is_none());
    }

    #[test]
    fn cycle_iter_forward() {
        let items = [0, 1, 2, 3, 4, 5, 6, 7];
        let from_5 = super::cycle_forward(&items, 5)
            .map(|(_, val)| *val)
            .collect::<Vec<_>>();
        assert_eq!(from_5, &[6, 7, 0, 1, 2, 3, 4, 5]);
        let from_last = super::cycle_forward(&items, 7)
            .map(|(_, val)| *val)
            .collect::<Vec<_>>();
        assert_eq!(from_last, &items);
        // Don't panic on empty slice
        let _ = super::cycle_forward::<i32>(&[], 5).count();
    }

    #[test]
    fn cycle_iter_backward() {
        let items = [0, 1, 2, 3, 4, 5, 6, 7];
        let from_5 = super::cycle_backward(&items, 5)
            .map(|(_, val)| *val)
            .collect::<Vec<_>>();
        assert_eq!(from_5, &[4, 3, 2, 1, 0, 7, 6, 5]);
        let from_0 = super::cycle_backward(&items, 0)
            .map(|(_, val)| *val)
            .collect::<Vec<_>>();
        assert_eq!(from_0, &[7, 6, 5, 4, 3, 2, 1, 0]);
        // Don't panic on empty slice
        let _ = super::cycle_backward::<i32>(&[], 5).count();
    }
}
