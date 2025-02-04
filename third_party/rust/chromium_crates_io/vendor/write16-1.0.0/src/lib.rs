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

//! `write16` provides the trait `Write16`, which a UTF-16 analog of the
//! `core::fmt::Write` trait (the sink part—not the formatting part).

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "arrayvec")]
extern crate arrayvec;
#[cfg(feature = "smallvec")]
extern crate smallvec;

/// A UTF-16 sink analogous to `core::fmt::Write`.
pub trait Write16 {
    /// Write a slice containing UTF-16 to the sink.
    ///
    /// The implementor of the trait should not validate UTF-16.
    /// It's the responsibility of the caller to pass valid
    /// UTF-16.
    fn write_slice(&mut self, s: &[u16]) -> core::fmt::Result;

    /// Write a Unicode scalar value to the sink.
    #[inline(always)]
    fn write_char(&mut self, c: char) -> core::fmt::Result {
        let mut buf = [0u16; 2];
        self.write_slice(c.encode_utf16(&mut buf))
    }

    /// A hint that the caller expects to write `upcoming` UTF-16
    /// code units. The implementation must not assume `upcoming`
    /// to be exact. The caller may write more or fewer code units
    /// using `write_slice()` and `write_char()`. However, the
    /// caller should try to give reasonable estimates if it uses
    /// this method.
    ///
    /// For `Vec` and `SmallVec`, this maps to `reserve()`.
    /// The default implementation does nothing.
    #[inline(always)]
    fn size_hint(&mut self, upcoming: usize) -> core::fmt::Result {
        let _ = upcoming;
        Ok(())
    }
}

#[cfg(feature = "alloc")]
impl Write16 for alloc::vec::Vec<u16> {
    #[inline(always)]
    fn write_slice(&mut self, s: &[u16]) -> core::fmt::Result {
        self.extend_from_slice(s);
        Ok(())
    }

    #[inline(always)]
    fn write_char(&mut self, c: char) -> core::fmt::Result {
        if c <= '\u{FFFF}' {
            self.push(c as u16);
        } else {
            let mut buf = [0u16; 2];
            let u = u32::from(c);
            buf[0] = (0xD7C0 + (u >> 10)) as u16;
            buf[1] = (0xDC00 + (u & 0x3FF)) as u16;
            self.extend_from_slice(&mut buf);
        }
        Ok(())
    }

    #[inline(always)]
    fn size_hint(&mut self, upcoming: usize) -> core::fmt::Result {
        self.reserve(upcoming);
        Ok(())
    }
}

#[cfg(feature = "smallvec")]
impl<A: smallvec::Array<Item = u16>> Write16 for smallvec::SmallVec<A> {
    #[inline(always)]
    fn write_slice(&mut self, s: &[u16]) -> core::fmt::Result {
        self.extend_from_slice(s);
        Ok(())
    }

    #[inline(always)]
    fn write_char(&mut self, c: char) -> core::fmt::Result {
        if c <= '\u{FFFF}' {
            self.push(c as u16);
        } else {
            let mut buf = [0u16; 2];
            let u = u32::from(c);
            buf[0] = (0xD7C0 + (u >> 10)) as u16;
            buf[1] = (0xDC00 + (u & 0x3FF)) as u16;
            self.extend_from_slice(&mut buf);
        }
        Ok(())
    }

    #[inline(always)]
    fn size_hint(&mut self, upcoming: usize) -> core::fmt::Result {
        self.reserve(upcoming);
        Ok(())
    }
}

#[cfg(feature = "arrayvec")]
impl<const CAP: usize> Write16 for arrayvec::ArrayVec<u16, CAP> {
    #[inline(always)]
    fn write_slice(&mut self, s: &[u16]) -> core::fmt::Result {
        if self.try_extend_from_slice(s).is_ok() {
            Ok(())
        } else {
            Err(core::fmt::Error {})
        }
    }

    #[inline(always)]
    fn write_char(&mut self, c: char) -> core::fmt::Result {
        if c <= '\u{FFFF}' {
            if self.try_push(c as u16).is_ok() {
                Ok(())
            } else {
                Err(core::fmt::Error {})
            }
        } else {
            let mut buf = [0u16; 2];
            let u = u32::from(c);
            buf[0] = (0xD7C0 + (u >> 10)) as u16;
            buf[1] = (0xDC00 + (u & 0x3FF)) as u16;
            self.write_slice(&mut buf)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::Write16;

    #[cfg(feature = "alloc")]
    #[test]
    fn test_vec() {
        let mut v: alloc::vec::Vec<u16> = alloc::vec::Vec::new();
        assert_eq!(v.capacity(), 0);
        assert!(v.size_hint(32).is_ok());
        assert!(v.capacity() >= 32);
        assert_eq!(v.len(), 0);
        assert!(v.write_slice([0x0061u16, 0x0062u16].as_slice()).is_ok());
        assert_eq!(v.len(), 2);
        assert!(v.write_char('☃').is_ok());
        assert_eq!(v.len(), 3);
        assert!(v.write_char('😊').is_ok());
        assert_eq!(v.len(), 5);
        assert_eq!(
            v.as_slice(),
            [0x0061u16, 0x0062u16, 0x2603u16, 0xD83Du16, 0xDE0Au16].as_slice()
        );
    }

    #[cfg(feature = "smallvec")]
    #[test]
    fn test_smallvec() {
        let mut v: smallvec::SmallVec<[u16; 2]> = smallvec::SmallVec::new();
        assert_eq!(v.capacity(), 2);
        assert!(v.size_hint(32).is_ok());
        assert!(v.capacity() >= 32);
        assert_eq!(v.len(), 0);
        assert!(v.write_slice([0x0061u16, 0x0062u16].as_slice()).is_ok());
        assert_eq!(v.len(), 2);
        assert!(v.write_char('☃').is_ok());
        assert_eq!(v.len(), 3);
        assert!(v.write_char('😊').is_ok());
        assert_eq!(v.len(), 5);
        assert_eq!(
            v.as_slice(),
            [0x0061u16, 0x0062u16, 0x2603u16, 0xD83Du16, 0xDE0Au16].as_slice()
        );
    }

    #[cfg(feature = "arrayvec")]
    #[test]
    fn test_arrayvec() {
        let mut v: arrayvec::ArrayVec<u16, 4> = arrayvec::ArrayVec::new();
        assert_eq!(v.capacity(), 4);
        assert!(v.size_hint(32).is_ok());
        assert_eq!(v.capacity(), 4);
        assert_eq!(v.len(), 0);
        assert!(v.write_char('😊').is_ok());
        assert_eq!(v.len(), 2);
        assert!(v.write_char('☃').is_ok());
        assert_eq!(v.len(), 3);
        assert!(v.write_char('😊').is_err());
        assert_eq!(v.len(), 3);
        assert_eq!(v.as_slice(), [0xD83Du16, 0xDE0Au16, 0x2603u16].as_slice());
    }
}
