//! A type that represents the union of a set of regular expressions.

use regex::RegexSet as RxSet;
use std::cell::Cell;

/// A dynamic set of regular expressions.
#[derive(Debug, Default)]
pub struct RegexSet {
    items: Vec<String>,
    /// Whether any of the items in the set was ever matched. The length of this
    /// vector is exactly the length of `items`.
    matched: Vec<Cell<bool>>,
    set: Option<RxSet>,
    /// Whether we should record matching items in the `matched` vector or not.
    record_matches: bool,
}

impl RegexSet {
    /// Is this set empty?
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// Insert a new regex into this set.
    pub fn insert<S>(&mut self, string: S)
    where
        S: AsRef<str>,
    {
        self.items.push(string.as_ref().to_owned());
        self.matched.push(Cell::new(false));
        self.set = None;
    }

    /// Returns slice of String from its field 'items'
    pub fn get_items(&self) -> &[String] {
        &self.items[..]
    }

    /// Returns an iterator over regexes in the set which didn't match any
    /// strings yet.
    pub fn unmatched_items(&self) -> impl Iterator<Item = &String> {
        self.items.iter().enumerate().filter_map(move |(i, item)| {
            if !self.record_matches || self.matched[i].get() {
                return None;
            }

            Some(item)
        })
    }

    /// Construct a RegexSet from the set of entries we've accumulated.
    ///
    /// Must be called before calling `matches()`, or it will always return
    /// false.
    pub fn build(&mut self, record_matches: bool) {
        let items = self.items.iter().map(|item| format!("^{}$", item));
        self.record_matches = record_matches;
        self.set = match RxSet::new(items) {
            Ok(x) => Some(x),
            Err(e) => {
                warn!("Invalid regex in {:?}: {:?}", self.items, e);
                None
            }
        }
    }

    /// Does the given `string` match any of the regexes in this set?
    pub fn matches<S>(&self, string: S) -> bool
    where
        S: AsRef<str>,
    {
        let s = string.as_ref();
        let set = match self.set {
            Some(ref set) => set,
            None => return false,
        };

        if !self.record_matches {
            return set.is_match(s);
        }

        let matches = set.matches(s);
        if !matches.matched_any() {
            return false;
        }
        for i in matches.iter() {
            self.matched[i].set(true);
        }

        true
    }
}
