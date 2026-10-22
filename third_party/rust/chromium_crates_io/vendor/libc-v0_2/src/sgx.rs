//! SGX C types definition

use crate::prelude::*;

pub type intmax_t = i64;
pub type uintmax_t = u64;

pub type size_t = usize;
pub type ptrdiff_t = isize;
pub type intptr_t = isize;
pub type uintptr_t = usize;
pub type ssize_t = isize;

#[deprecated(since = "0.2.190", note = "Use `c_int::MIN` instead.")]
pub const INT_MIN: c_int = c_int::MIN;
#[deprecated(since = "0.2.190", note = "Use `c_int::MAX` instead.")]
pub const INT_MAX: c_int = c_int::MAX;
