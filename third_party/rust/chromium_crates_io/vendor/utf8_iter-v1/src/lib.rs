// Copyright Mozilla Foundation
//
// Licensed under the Apache License (Version 2.0), or the MIT license,
// (the "Licenses") at your option. You may not use this file except in
// compliance with one of the Licenses. You may obtain copies of the
// Licenses at:
//
//    https://www.apache.org/licenses/LICENSE-2.0
//    https://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Licenses is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the Licenses for the specific language governing permissions and
// limitations under the Licenses.

#![no_std]
#![warn(missing_docs)]
#![deny(unsafe_code)] // overridden in `str.rs`

//! Provides iteration by `char` over `&[u8]` containing potentially-invalid
//! UTF-8 such that errors are handled according to the [WHATWG Encoding
//! Standard](https://encoding.spec.whatwg.org/#utf-8-decoder) (i.e. the same
//! way as in `String::from_utf8_lossy`).
//!
//! The trait `Utf8CharsEx` provides the convenience method `chars()` on
//! byte slices themselves instead of having to use the more verbose
//! `Utf8Chars::new(slice)`.
//!
//! ```rust
//! use utf8_iter::Utf8CharsEx;
//! let data = b"\xFF\xC2\xE2\xE2\x98\xF0\xF0\x9F\xF0\x9F\x92\xE2\x98\x83";
//! let from_iter: String = data.chars().collect();
//! let from_std = String::from_utf8_lossy(data);
//! assert_eq!(from_iter, from_std);
//! ```

mod basic;
mod handler;
mod indices;
mod macros;
mod report;
mod str;

pub use crate::basic::DefaultHandler;
pub use crate::basic::Utf8CharIndices;
pub use crate::basic::Utf8Chars;
pub use crate::basic::Utf8CharsEx;
pub use crate::handler::Utf8CharsWithHandler;
pub use crate::handler::Utf8Handler;
pub use crate::indices::CharIndicesWithHandler;
pub use crate::indices::Utf8CharIndicesWithHandler;
pub use crate::report::ErrorReportingUtf8CharIndices;
pub use crate::report::ErrorReportingUtf8Chars;
pub use crate::report::Utf8CharsError;
pub use crate::str::CharsWithHandler;
