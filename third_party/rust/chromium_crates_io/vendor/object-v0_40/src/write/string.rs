use alloc::vec::Vec;

use crate::write::{Error, Result, WritableBuffer};

#[cfg(feature = "write_std")]
type IndexSet<K> = indexmap::IndexSet<K>;
#[cfg(not(feature = "write_std"))]
type IndexSet<K> = indexmap::IndexSet<K, hashbrown::DefaultHashBuilder>;

/// An identifier for an entry in a string table.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct StringId(u32);

/// A string table containing null terminated byte strings.
#[derive(Debug, Default)]
pub struct StringTable<'a> {
    strings: IndexSet<&'a [u8]>,
    offsets: Vec<u32>,
    size: u64,
    in_order: bool,
    // Only set for in_order.
    base: u32,
}

impl<'a> StringTable<'a> {
    /// Construct an empty string table.
    pub fn new() -> Self {
        StringTable::default()
    }

    /// Construct an empty string table that writes the strings in
    /// the order they are added.
    ///
    /// This does not perform suffix merging.
    pub fn new_in_order(base: u32) -> Self {
        StringTable {
            strings: IndexSet::default(),
            offsets: Vec::new(),
            size: base.into(),
            in_order: true,
            base,
        }
    }

    /// Return true if the string table contains no strings.
    pub fn is_empty(&self) -> bool {
        self.strings.is_empty()
    }

    /// Add a string to the string table.
    ///
    /// Duplicate strings return the id of the existing entry.
    ///
    /// Must be called before [`Self::write`].
    ///
    /// The string must not contain a null byte; this is asserted here for
    /// debug builds, and checked by `write` for all builds.
    pub fn add(&mut self, string: &'a [u8]) -> StringId {
        debug_assert!(self.in_order || self.offsets.is_empty());
        debug_assert!(!string.contains(&0));
        let (id, new) = self.strings.insert_full(string);
        if new && self.in_order {
            self.offsets.push(self.size as u32);
            self.size += string.len() as u64 + 1;
        }
        StringId(id as u32)
    }

    /// Return the id of a previously added string.
    ///
    /// Panics if the string is not in the string table.
    pub fn get_id(&self, string: &[u8]) -> StringId {
        let id = self.strings.get_index_of(string).unwrap();
        StringId(id as u32)
    }

    /// Return the string for the given id.
    ///
    /// Panics if `id` is invalid.
    pub fn get_string(&self, id: StringId) -> &'a [u8] {
        self.strings.get_index(id.0 as usize).unwrap()
    }

    /// Return the offset of the given string.
    ///
    /// Must be called after [`Self::write`] unless the string table was
    /// constructed with [`Self::new_in_order`].
    ///
    /// When using `new_in_order`, the offset returned will be truncated if it
    /// overflows `u32`. Reporting the overflow is postponed to `write`.
    ///
    /// Panics if `id` is invalid or `write` was not called.
    pub fn get_offset(&self, id: StringId) -> u32 {
        self.offsets[id.0 as usize]
    }

    /// Return the offset of an optional string.
    ///
    /// Returns 0 if `id` is `None`. Otherwise see [`Self::get_offset`].
    pub fn maybe_get_offset(&self, id: Option<StringId>) -> u32 {
        let Some(id) = id else {
            return 0;
        };
        self.get_offset(id)
    }

    /// Append the string table to the given buffer, and
    /// calculate the list of string offsets.
    ///
    /// `base` is the initial string table offset. For example,
    /// this should be 1 for ELF, to account for the initial
    /// null byte (which must have been written by the caller).
    ///
    /// Returns the total size, including base.
    ///
    /// Returns an error if:
    /// - `write` has already been called
    /// - any string contains a null byte
    /// - the string table size is > `u32::MAX`
    pub fn write<W: WritableBuffer + ?Sized>(&mut self, w: &mut W, base: u32) -> Result<u32> {
        if !self.in_order && !self.offsets.is_empty() {
            return Err(Error("string table already written".into()));
        }
        if self.strings.iter().any(|s| s.contains(&0)) {
            return Err(Error("string table entry contains null byte".into()));
        }

        if self.in_order {
            debug_assert_eq!(self.base, base);
            for string in &self.strings {
                w.write_bytes(string);
                w.write_bytes(&[0]);
            }
            return u32::try_from(self.size)
                .map_err(|_| Error("string table size overflow".into()));
        }

        let mut ids: Vec<_> = (0..self.strings.len()).collect();
        sort(&mut ids, 1, &self.strings);

        self.offsets = vec![0; ids.len()];
        let mut offset = u64::from(base);
        let mut previous = &[][..];
        for id in ids {
            let string = self.strings.get_index(id).unwrap();
            let len = string.len() as u64 + 1;
            if previous.ends_with(string) {
                self.offsets[id] = (offset - len) as u32;
            } else {
                self.offsets[id] = offset as u32;
                w.write_bytes(string);
                w.write_bytes(&[0]);
                offset += len;
                previous = string;
            }
        }
        u32::try_from(offset).map_err(|_| Error("string table size overflow".into()))
    }

    /// Calculate the size in bytes of the string table.
    ///
    /// `base` is the initial string table offset. For example,
    /// this should be 1 for ELF, to account for the initial
    /// null byte.
    #[cfg(all(feature = "build_core", feature = "elf"))]
    pub(crate) fn size(&self, base: u32) -> u64 {
        if self.in_order {
            debug_assert_eq!(self.base, base);
            return self.size;
        }

        // TODO: cache this result?
        let mut ids: Vec<_> = (0..self.strings.len()).collect();
        sort(&mut ids, 1, &self.strings);

        let mut size = base as u64;
        let mut previous = &[][..];
        for id in ids {
            let string = self.strings.get_index(id).unwrap();
            if !previous.ends_with(string) {
                size += string.len() as u64 + 1;
                previous = string;
            }
        }
        size
    }
}

// Multi-key quicksort.
//
// Ordering is such that if a string is a suffix of at least one other string,
// then it is placed immediately after one of those strings. That is:
// - comparison starts at the end of the string
// - shorter strings come later
//
// Based on the implementation in LLVM.
fn sort(mut ids: &mut [usize], mut pos: usize, strings: &IndexSet<&[u8]>) {
    loop {
        if ids.len() <= 1 {
            return;
        }

        let pivot = byte(ids[0], pos, strings);
        let mut lower = 0;
        let mut upper = ids.len();
        let mut i = 1;
        while i < upper {
            let b = byte(ids[i], pos, strings);
            if b > pivot {
                ids.swap(lower, i);
                lower += 1;
                i += 1;
            } else if b < pivot {
                upper -= 1;
                ids.swap(upper, i);
            } else {
                i += 1;
            }
        }

        sort(&mut ids[..lower], pos, strings);
        sort(&mut ids[upper..], pos, strings);

        if pivot == 0 {
            return;
        }
        ids = &mut ids[lower..upper];
        pos += 1;
    }
}

fn byte(id: usize, pos: usize, strings: &IndexSet<&[u8]>) -> u8 {
    let string = strings.get_index(id).unwrap();
    let len = string.len();
    if len >= pos {
        string[len - pos]
    } else {
        // We know the strings don't contain null bytes.
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn string_table() {
        let mut table = StringTable::default();
        let id0 = table.add(b"");
        let id1 = table.add(b"foo");
        let id2 = table.add(b"bar");
        let id3 = table.add(b"foobar");

        let mut data = vec![0];
        assert_eq!(table.write(&mut data, 1), Ok(12));
        assert_eq!(data, b"\0foobar\0foo\0");

        assert_eq!(table.get_offset(id0), 11);
        assert_eq!(table.get_offset(id1), 8);
        assert_eq!(table.get_offset(id2), 4);
        assert_eq!(table.get_offset(id3), 1);

        let mut data = Vec::new();
        assert!(table.write(&mut data, 1).is_err());
    }

    #[test]
    fn string_table_in_order() {
        let mut table = StringTable::new_in_order(1);
        let id0 = table.add(b"");
        let id1 = table.add(b"foo");
        let id2 = table.add(b"bar");
        let id3 = table.add(b"foobar");

        let mut data = vec![0];
        assert_eq!(table.write(&mut data, 1), Ok(17));
        assert_eq!(data, b"\0\0foo\0bar\0foobar\0");

        assert_eq!(table.get_offset(id0), 1);
        assert_eq!(table.get_offset(id1), 2);
        assert_eq!(table.get_offset(id2), 6);
        assert_eq!(table.get_offset(id3), 10);

        // Not documented or expected to be needed, but multiple writes aren't prevented.
        let mut data2 = vec![0];
        assert_eq!(table.write(&mut data2, 1), Ok(17));
        assert_eq!(data, data2);
    }
}
