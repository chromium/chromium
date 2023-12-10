#[cfg(no_str_strip_prefix)] // rustc <1.45
pub(crate) trait StripPrefixExt {
    fn strip_prefix(&self, ch: char) -> Option<&str>;
}

#[cfg(no_str_strip_prefix)]
impl StripPrefixExt for str {
    fn strip_prefix(&self, ch: char) -> Option<&str> {
        if self.starts_with(ch) {
            Some(&self[ch.len_utf8()..])
        } else {
            None
        }
    }
}

pub(crate) use crate::alloc::vec::Vec;

#[cfg(no_alloc_crate)] // rustc <1.36
pub(crate) mod alloc {
    pub use std::alloc;
    pub use std::vec;
}
