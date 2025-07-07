use std::fmt::{self, Display, Write};
use std::mem::size_of;

use anyhow::{anyhow, Result};
use bytemuck::{NoUninit, Pod as PodTrait};
use bytemuck_derive::{Pod, Zeroable};

#[derive(Clone, Copy, PartialEq, Eq, Debug, Zeroable, Pod)]
#[repr(C)]
pub struct U32Pair(pub u32, pub u32);

pub fn clone_vec_as_bytes<T: NoUninit>(input: &[T]) -> Vec<u8> {
    bytemuck::cast_slice(input).to_vec()
}

pub fn vec_from_bytes<T: PodTrait>(bytes: &[u8]) -> Vec<T> {
    if bytes.len() % size_of::<T>() != 0 {
        panic!(
            "vecT: got {} bytes, needed multiple of {}",
            bytes.len(),
            size_of::<T>()
        );
    }
    bytemuck::cast_slice(bytes).to_vec()
}

pub fn limit_str(s: &str, max_len: usize) -> String {
    limit_bytes(s.as_bytes(), max_len)
}

pub fn limit_bytes(s: &[u8], max_len: usize) -> String {
    if s.len() > max_len {
        format!("{}...", String::from_utf8_lossy(&s[0..max_len]))
    } else {
        String::from_utf8_lossy(s).to_string()
    }
}

pub fn to_hex_string(bytes: &[u8]) -> String {
    bytes
        .iter()
        .map(|b| format!("{b:02x}"))
        .collect::<Vec<_>>()
        .join("")
}

pub fn from_hex_string(s: &str) -> Result<Vec<u8>> {
    let mut result = Vec::with_capacity(s.len() / 2);
    let mut iter = s.chars();
    while let Some(c1) = iter.next() {
        let c2 = iter
            .next()
            .ok_or_else(|| anyhow!("expecting even number of chars"))?;
        let byte = u8::from_str_radix(&format!("{c1}{c2}"), 16)?;
        result.push(byte);
    }
    Ok(result)
}

struct LimitedWriter<'a> {
    buf: &'a mut Vec<u8>,
    max_len: usize,
}

impl<'a> LimitedWriter<'a> {
    fn new(buf: &'a mut Vec<u8>, max_len: usize) -> Self {
        Self { buf, max_len }
    }
}

impl Write for LimitedWriter<'_> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let remaining = self.max_len.saturating_sub(self.buf.len());
        if s.len() > remaining {
            self.buf.extend_from_slice(&s.as_bytes()[..remaining]);
            Err(fmt::Error)
        } else {
            self.buf.extend_from_slice(s.as_bytes());
            Ok(())
        }
    }
}

pub fn limit_display(obj: impl Display, max_len: usize) -> String {
    let mut buffer = Vec::new();
    let mut writer = LimitedWriter::new(&mut buffer, max_len);

    let r = write!(writer, "{obj}");
    let mut exceeded = r.is_err();
    let mut valid_str = match String::from_utf8(buffer) {
        Ok(s) => s,
        Err(e) => {
            exceeded = true;
            let l = e.utf8_error().valid_up_to();
            let mut buf = e.into_bytes();
            buf.truncate(l);
            String::from_utf8(buf).unwrap()
        }
    };

    if exceeded {
        valid_str.push_str("...");
    }
    valid_str
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_short_string() {
        let result = limit_display("hello", 10);
        assert_eq!(result, "hello");
    }

    #[test]
    fn test_exact_length() {
        let result = limit_display("1234567890", 10);
        assert_eq!(result, "1234567890");
    }

    #[test]
    fn test_truncate_with_ellipsis() {
        let result = limit_display("This is a long string", 10);
        assert_eq!(result, "This is a ...");
    }

    #[test]
    fn test_utf8_truncation() {
        let result = limit_display("😀😀😀😀😀", 10);
        assert_eq!(result, "😀😀...");
    }

    #[test]
    fn test_utf8_partial_char() {
        let result = limit_display("😀😀😀", 7);
        assert_eq!(result, "😀...");
    }

    #[test]
    fn test_empty_string() {
        let result = limit_display("", 10);
        assert_eq!(result, "");
    }

    #[test]
    fn test_very_small_limit() {
        let result = limit_display("hello", 1);
        assert_eq!(result, "h...");
    }
}
