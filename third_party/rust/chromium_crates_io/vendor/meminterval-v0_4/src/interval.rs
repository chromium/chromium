use core::{
    cmp::{max, min, Eq, Ord, Ordering, PartialEq, PartialOrd},
    fmt::{Debug, Display},
    ops::{Range, RangeInclusive},
};
use num_traits::{CheckedAdd, One};
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Interval<T: Ord> {
    pub start: T,
    pub end: T,
}

impl<T: Ord> Interval<T> {
    pub fn new(start: T, end: T) -> Self {
        Interval { start, end }
    }

    pub fn is_valid(&self) -> bool {
        self.start < self.end
    }
}

impl<T: Ord + Clone> Interval<T> {
    pub fn intersect(&self, other: &Self) -> Option<Self> {
        let result = Interval::new(
            max(self.start.clone(), other.start.clone()),
            min(self.end.clone(), other.end.clone()),
        );
        if result.is_valid() {
            Some(result)
        } else {
            None
        }
    }
}

impl<T: Ord> PartialEq for Interval<T> {
    fn eq(&self, other: &Self) -> bool {
        self.start == other.start && self.end == other.end
    }
}

impl<T: Ord> Eq for Interval<T> {}

impl<T: Ord> PartialOrd for Interval<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(match self.start.cmp(&other.start) {
            Ordering::Equal => self.end.cmp(&other.end),
            ord => ord,
        })
    }
}

impl<T: Ord> Ord for Interval<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.partial_cmp(other).unwrap()
    }
}

impl<T: Ord + Display> Display for Interval<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}..{}", self.start, self.end)
    }
}

impl<T: Ord + Clone> From<Range<T>> for Interval<T> {
    fn from(range: Range<T>) -> Self {
        Interval::new(range.start.clone(), range.end.clone())
    }
}

impl<T: Ord + Clone> From<&Range<T>> for Interval<T> {
    fn from(range: &Range<T>) -> Self {
        Interval::new(range.start.clone(), range.end.clone())
    }
}

impl<T: Ord + Clone + CheckedAdd + One> From<RangeInclusive<T>> for Interval<T> {
    fn from(range: RangeInclusive<T>) -> Self {
        Interval::new(range.start().clone(), range.end().clone() + T::one())
    }
}

impl<T: Ord + Clone + CheckedAdd + One> From<&RangeInclusive<T>> for Interval<T> {
    fn from(range: &RangeInclusive<T>) -> Self {
        Interval::new(range.start().clone(), range.end().clone() + T::one())
    }
}
