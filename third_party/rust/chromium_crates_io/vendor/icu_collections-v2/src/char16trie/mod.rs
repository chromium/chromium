// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module provides a data structure for a space-efficient and time-efficient lookup of
//! sequences of 16-bit units (commonly but not necessarily UTF-16 code units)
//! which map to integer values.
//!
//! It is an implementation of the existing [ICU4C UCharsTrie](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/classicu_1_1UCharsTrie.html)
//! / [ICU4J CharsTrie](https://unicode-org.github.io/icu-docs/apidoc/released/icu4j/com/ibm/icu/util/CharsTrie.html) API.
//!
//! ## Architecture
//!
//! ICU4X [`Char16Trie`] is designed to provide a read-only view of `UCharsTrie` data that is exported from ICU4C.
//!
//! ## Examples
//!
//! ### Querying a `Char16Trie`
//!
//! ```rust
//! use icu::collections::char16trie::Char16Trie;
//! use zerovec::ZeroVec;
//!
//! // A Char16Trie containing the ASCII characters mapping 'a' to 1 and 'ab'
//! // to 100.
//! let trie_data = [48, 97, 176, 98, 32868];
//! let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(&trie_data));
//!
//! let mut cursor = trie.cursor();
//! cursor.step('a');
//! assert_eq!(cursor.value(), Some(1));
//! assert!(!cursor.is_empty());
//! cursor.step('b');
//! assert_eq!(cursor.value(), Some(100));
//! assert!(cursor.is_empty());
//! cursor.step('c');
//! assert_eq!(cursor.value(), None);
//! assert!(cursor.is_empty());
//! ```
//!
//! [`ICU4X`]: ../icu/index.html

mod trie;

pub use trie::Char16Trie;
pub use trie::Char16TrieCursor;
#[allow(deprecated)]
pub use trie::Char16TrieIterator;
#[allow(deprecated)]
pub use trie::TrieResult;
