use std::borrow::Cow;
use std::borrow::Cow::Borrowed;
use std::cmp::{max, min, Ordering};

use crate::token::{TOKEN_EOF, TOKEN_EPSILON};
use crate::vocabulary::{Vocabulary, DUMMY_VOCAB};

/// Represents interval equivalent to `a..=b`
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct Interval {
    /// start
    pub a: i32,
    /// end >= start
    pub b: i32,
}

pub(crate) const INVALID: Interval = Interval { a: -1, b: -2 };

impl Interval {
    /* stop is not included! */
    fn new(a: i32, b: i32) -> Interval {
        Interval { a, b }
    }

    // fn contains(&self, _item: i32) -> bool { unimplemented!() }

    fn length(&self) -> i32 {
        self.b - self.a
    }

    fn union(&self, another: &Interval) -> Interval {
        Interval {
            a: min(self.a, another.a),
            b: max(self.b, another.b),
        }
    }

    /** Does self start completely before other? Disjoint */
    pub fn starts_before_disjoint(&self, other: &Interval) -> bool {
        self.a < other.a && self.b < other.a
    }

    /** Does self start at or before other? Nondisjoint */
    pub fn starts_before_non_disjoint(&self, other: &Interval) -> bool {
        self.a <= other.a && self.b >= other.a
    }

    /** Does self.a start after other.b? May or may not be disjoint */
    pub fn starts_after(&self, other: &Interval) -> bool {
        self.a > other.a
    }

    /** Does self start completely after other? Disjoint */
    pub fn starts_after_disjoint(&self, other: &Interval) -> bool {
        self.a > other.b
    }

    /** Does self start after other? NonDisjoint */
    pub fn starts_after_non_disjoint(&self, other: &Interval) -> bool {
        self.a > other.a && self.a <= other.b // self.b>=other.b implied
    }

    /** Are both ranges disjoint? I.e., no overlap? */
    pub fn disjoint(&self, other: &Interval) -> bool {
        self.starts_before_disjoint(other) || self.starts_after_disjoint(other)
    }

    /** Are two intervals adjacent such as 0..41 and 42..42? */
    pub fn adjacent(&self, other: &Interval) -> bool {
        self.a == other.b + 1 || self.b == other.a - 1
    }

    //    public boolean properlyContains(Interval other) {
    //    return other.a >= self.a && other.b <= self.b;
    //    }
    //
    //    /** Return the interval computed from combining self and other */
    //    public Interval union(Interval other) {
    //    return Interval.of(Math.min(a, other.a), Math.max(b, other.b));
    //    }
    //
    //    /** Return the interval in common between self and o */
    //    public Interval intersection(Interval other) {
    //    return Interval.of(Math.max(a, other.a), Math.min(b, other.b));
    //    }
}

/// Set of disjoint intervals
///
/// Basically a set of integers but optimized for cases when it is sparse and created by adding
/// intervals of integers.
#[derive(Clone, Eq, PartialEq, Debug)]
pub struct IntervalSet {
    intervals: Vec<Interval>,
    #[allow(missing_docs)]
    pub read_only: bool,
}

#[allow(missing_docs)]
impl Default for IntervalSet {
    fn default() -> Self {
        Self::new()
    }
}

impl IntervalSet {
    pub fn new() -> IntervalSet {
        IntervalSet {
            intervals: Vec::new(),
            read_only: false,
        }
    }

    pub fn get_min(&self) -> Option<i32> {
        self.intervals.first().map(|x| x.a)
    }

    pub fn add_one(&mut self, _v: i32) {
        self.add_range(_v, _v)
    }

    pub fn add_range(&mut self, l: i32, h: i32) {
        self.add_interval(Interval { a: l, b: h })
    }

    pub fn add_interval(&mut self, added: Interval) {
        if added.length() < 0 {
            return;
        }

        let mut i = 0;
        while let Some(r) = self.intervals.get_mut(i) {
            if *r == added {
                return;
            }

            if added.adjacent(r) || !added.disjoint(r) {
                // next to each other, make a single larger interval
                let bigger = added.union(r);
                *r = bigger;
                // make sure we didn't just create an interval that
                // should be merged with next interval in list
                loop {
                    i += 1;
                    let next = match self.intervals.get(i) {
                        Some(v) => v,
                        None => break,
                    };
                    if !bigger.adjacent(next) && bigger.disjoint(next) {
                        break;
                    }

                    // if we bump up against or overlap next, merge
                    self.intervals[i - 1] = bigger.union(next); // set to 3 merged ones
                    self.intervals.remove(i);
                }
                return;
            }
            if added.starts_before_disjoint(r) {
                // insert before r
                self.intervals.insert(i, added);
                return;
            }
            i += 1;
        }

        self.intervals.push(added);
    }

    pub fn add_set(&mut self, _other: &IntervalSet) {
        for i in &_other.intervals {
            self.add_interval(*i)
        }
    }

    pub fn substract(&mut self, right: &IntervalSet) {
        let result = self;
        let mut result_i = 0usize;
        let mut right_i = 0usize;

        while result_i < result.intervals.len() && right_i < right.intervals.len() {
            let result_interval = result.intervals[result_i];
            let right_interval = right.intervals[right_i];

            if right_interval.b < result_interval.a {
                right_i += 1;
                continue;
            }

            if right_interval.a > result_interval.b {
                result_i += 1;
                continue;
            }

            let before_curr = if right_interval.a > result_interval.a {
                Some(Interval::new(result_interval.a, right_interval.a - 1))
            } else {
                None
            };
            let after_curr = if right_interval.b < result_interval.b {
                Some(Interval::new(right_interval.b + 1, result_interval.b))
            } else {
                None
            };

            match (before_curr, after_curr) {
                (Some(before_curr), Some(after_curr)) => {
                    result.intervals[result_i] = before_curr;
                    result.intervals.insert(result_i + 1, after_curr);
                    result_i += 1;
                    right_i += 1;
                }
                (Some(before_curr), None) => {
                    result.intervals[result_i] = before_curr;
                    result_i += 1;
                }
                (None, Some(after_curr)) => {
                    result.intervals[result_i] = after_curr;
                    right_i += 1;
                }
                (None, None) => {
                    result.intervals.remove(result_i);
                }
            }
        }

        //        return result;
    }

    pub fn complement(&self, start: i32, stop: i32) -> IntervalSet {
        let mut vocablulary_is = IntervalSet::new();
        vocablulary_is.add_range(start, stop);
        vocablulary_is.substract(self);
        vocablulary_is
    }

    pub fn contains(&self, _item: i32) -> bool {
        self.intervals
            .binary_search_by(|x| {
                if _item < x.a {
                    return Ordering::Greater;
                }
                if _item > x.b {
                    return Ordering::Less;
                }
                Ordering::Equal
            })
            .is_ok()
    }

    pub fn length(&self) -> i32 {
        self.intervals
            .iter()
            .fold(0, |acc, it| acc + it.b - it.a + 1)
    }

    // fn remove_range(&self, _v: &Interval) { unimplemented!() }

    pub fn remove_one(&mut self, el: i32) {
        if self.read_only {
            panic!("can't alter readonly IntervalSet")
        }

        for i in 0..self.intervals.len() {
            let int = &mut self.intervals[i];
            if el < int.a {
                break;
            }

            if el == int.a && el == int.b {
                self.intervals.remove(i);
                break;
            }

            if el == int.a {
                int.a += 1;
                break;
            }

            if el == int.b {
                int.b -= 1;
                break;
            }

            if el > int.a && el < int.b {
                let old_b = int.b;
                int.b = el - 1;
                self.add_range(el + 1, old_b);
            }
        }
    }

    //    fn String(&self) -> String {
    //        unimplemented!()
    //    }
    //
    //    fn String_verbose(
    //        &self,
    //        _literalNames: Vec<String>,
    //        _symbolicNames: Vec<String>,
    //        _elemsAreChar: bool,
    //    ) -> String {
    //        unimplemented!()
    //    }
    //
    //    fn to_char_String(&self) -> String {
    //        unimplemented!()
    //    }
    //
    pub fn to_index_string(&self) -> String {
        self.to_token_string(&DUMMY_VOCAB)
    }

    pub fn to_token_string(&self, vocabulary: &dyn Vocabulary) -> String {
        if self.intervals.is_empty() {
            return "{}".to_owned();
        }
        let mut buf = String::new();
        if self.length() > 1 {
            buf += "{";
        }
        let mut iter = self.intervals.iter();
        while let Some(int) = iter.next() {
            if int.a == int.b {
                buf += self.element_name(vocabulary, int.a).as_ref();
            } else {
                for i in int.a..(int.b + 1) {
                    if i > int.a {
                        buf += ", ";
                    }
                    buf += self.element_name(vocabulary, i).as_ref();
                }
            }
            if iter.len() > 0 {
                buf += ", ";
            }
        }

        if self.length() > 1 {
            buf += "}";
        }

        buf
    }

    fn element_name<'a>(&self, vocabulary: &'a dyn Vocabulary, a: i32) -> Cow<'a, str> {
        if a == TOKEN_EOF {
            Borrowed("<EOF>")
        } else if a == TOKEN_EPSILON {
            Borrowed("<EPSILON>")
        } else {
            vocabulary.get_display_name(a)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_add_1() {
        let mut set = IntervalSet::new();
        set.add_range(1, 2);
        assert_eq!(&set.intervals, &[Interval { a: 1, b: 2 }]);
        set.add_range(2, 3);
        assert_eq!(&set.intervals, &[Interval { a: 1, b: 3 }]);
        set.add_range(1, 5);
        assert_eq!(&set.intervals, &[Interval { a: 1, b: 5 }]);
    }

    #[test]
    fn test_add_2() {
        let mut set = IntervalSet::new();
        set.add_range(1, 3);
        set.add_range(5, 6);
        assert_eq!(
            &set.intervals,
            &[Interval { a: 1, b: 3 }, Interval { a: 5, b: 6 }]
        );
        set.add_range(3, 4);
        assert_eq!(&set.intervals, &[Interval { a: 1, b: 6 }]);
    }

    #[test]
    fn test_remove() {
        let mut set = IntervalSet::new();
        set.add_range(1, 5);
        set.remove_one(3);
        assert_eq!(
            &set.intervals,
            &[Interval { a: 1, b: 2 }, Interval { a: 4, b: 5 }]
        );
    }

    #[test]
    fn test_substract() {
        let mut set1 = IntervalSet::new();
        set1.add_range(1, 2);
        set1.add_range(4, 5);
        let mut set2 = IntervalSet::new();
        set2.add_range(2, 4);
        set1.substract(&set2);
        assert_eq!(
            &set1.intervals,
            &[Interval { a: 1, b: 1 }, Interval { a: 5, b: 5 }]
        );
    }
}
