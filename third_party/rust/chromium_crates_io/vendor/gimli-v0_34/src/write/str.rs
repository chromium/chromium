use alloc::vec::Vec;
use core::ops::{Deref, DerefMut};

use crate::common::{DebugLineStrOffset, DebugStrOffset, SectionId};
use crate::write::{BaseId, FnvIndexSet, Result, Section, Writer};

// Requirements:
// - values are `[u8]`, null bytes are not allowed
// - insertion returns a fixed id
// - inserting a duplicate returns the id of the existing value
// - able to convert an id to a section offset
// Optional?
// - able to get an existing value given an id
//
// Limitations of current implementation (using IndexSet):
// - inserting requires either an allocation for duplicates,
//   or a double lookup for non-duplicates
// - doesn't preserve offsets when updating an existing `.debug_str` section
macro_rules! define_string_table {
    ($name:ident, $id:ident, $section:ident, $offset:ident, $docs:expr) => {
        #[doc=$docs]
        #[derive(Debug, Default)]
        pub struct $name {
            base_id: BaseId,
            strings: FnvIndexSet<Vec<u8>>,
            offsets: Vec<$offset>,
            len: usize,
        }

        impl $name {
            /// Add a string to the string table and return its id.
            ///
            /// If the string already exists, then return the id of the existing string.
            ///
            /// # Panics
            ///
            /// Panics if `bytes` contains a null byte.
            pub fn add<T>(&mut self, bytes: T) -> $id
            where
                T: Into<Vec<u8>>,
            {
                let bytes = bytes.into();
                assert!(!bytes.contains(&0));
                let len = bytes.len();
                let (index, inserted) = self.strings.insert_full(bytes);
                if inserted {
                    self.offsets.push($offset(self.len));
                    self.len += len + 1;
                }
                $id::new(self.base_id, index)
            }

            /// Return the number of strings in the table.
            #[inline]
            pub fn count(&self) -> usize {
                self.strings.len()
            }

            /// Get a reference to a string in the table.
            ///
            /// # Panics
            ///
            /// Panics if `id` is invalid.
            pub fn get(&self, id: $id) -> &[u8] {
                debug_assert_eq!(self.base_id, id.base_id);
                self.strings.get_index(id.index).map(Vec::as_slice).unwrap()
            }

            /// Get the offset of a string in the table.
            ///
            /// # Panics
            ///
            /// Panics if `id` is invalid.
            pub fn offset(&self, id: $id) -> $offset {
                debug_assert_eq!(self.base_id, id.base_id);
                self.offsets[id.index]
            }

            /// Write the string table to the `.debug_str` section.
            pub fn write<W: Writer>(&self, w: &mut $section<W>) -> Result<()> {
                for bytes in self.strings.iter() {
                    w.write(bytes)?;
                    w.write_u8(0)?;
                }
                Ok(())
            }
        }
    };
}

define_id!(StringId, "An identifier for a string in a `StringTable`.");

define_string_table!(
    StringTable,
    StringId,
    DebugStr,
    DebugStrOffset,
    "A table of strings that will be stored in a `.debug_str` section."
);

define_section!(DebugStr, DebugStrOffset, "A writable `.debug_str` section.");

define_id!(
    LineStringId,
    "An identifier for a string in a `LineStringTable`."
);

define_string_table!(
    LineStringTable,
    LineStringId,
    DebugLineStr,
    DebugLineStrOffset,
    "A table of strings that will be stored in a `.debug_line_str` section."
);

define_section!(
    DebugLineStr,
    DebugLineStrOffset,
    "A writable `.debug_line_str` section."
);

#[cfg(test)]
#[cfg(feature = "read")]
mod tests {
    use super::*;
    use crate::LittleEndian;
    use crate::read;
    use crate::write::EndianVec;

    #[test]
    fn test_string_table() {
        let mut strings = StringTable::default();
        assert_eq!(strings.count(), 0);
        let id1 = strings.add(b"one");
        let id2 = strings.add(b"two");
        let id3 = strings.add([]);
        assert_eq!(strings.add(b"one"), id1);
        assert_eq!(strings.add(b"two"), id2);
        assert_eq!(strings.add([]), id3);
        assert_eq!(strings.get(id1), b"one");
        assert_eq!(strings.get(id2), b"two");
        assert_eq!(strings.get(id3), []);
        assert_eq!(strings.count(), 3);
        assert_eq!(strings.offset(id1), DebugStrOffset(0));
        assert_eq!(strings.offset(id2), DebugStrOffset(4));
        assert_eq!(strings.offset(id3), DebugStrOffset(8));

        let mut debug_str = DebugStr::from(EndianVec::new(LittleEndian));
        strings.write(&mut debug_str).unwrap();
        assert_eq!(debug_str.slice(), b"one\0two\0\0");

        let read_debug_str = read::DebugStr::new(debug_str.slice(), LittleEndian);
        let str1 = read_debug_str.get_str(strings.offset(id1)).unwrap();
        let str2 = read_debug_str.get_str(strings.offset(id2)).unwrap();
        let str3 = read_debug_str.get_str(strings.offset(id3)).unwrap();
        assert_eq!(str1.slice(), b"one");
        assert_eq!(str2.slice(), b"two");
        assert_eq!(str3.slice(), b"");
    }
}
