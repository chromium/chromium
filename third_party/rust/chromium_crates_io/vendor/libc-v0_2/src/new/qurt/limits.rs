//! Header: `limits.h`

use super::*;
use crate::prelude::*;

// Character properties
pub const CHAR_BIT: c_uint = 8;
#[deprecated(since = "0.2.190", note = "Use `c_char::MAX` instead.")]
pub const CHAR_MAX: c_char = c_char::MAX; // unsigned char on Hexagon
#[deprecated(since = "0.2.190", note = "Use `c_char::MIN` instead.")]
pub const CHAR_MIN: c_char = c_char::MIN;
#[deprecated(since = "0.2.190", note = "Use `i8::MAX` instead.")]
pub const SCHAR_MAX: i8 = i8::MAX;
#[deprecated(since = "0.2.190", note = "Use `i8::MIN` instead.")]
pub const SCHAR_MIN: i8 = i8::MIN;
#[deprecated(since = "0.2.190", note = "Use `c_uchar::MAX` instead.")]
pub const UCHAR_MAX: c_uchar = c_uchar::MAX;

// Integer properties
#[deprecated(since = "0.2.190", note = "Use `c_int::MAX` instead.")]
pub const INT_MAX: c_int = c_int::MAX;
#[deprecated(since = "0.2.190", note = "Use `c_int::MIN` instead.")]
pub const INT_MIN: c_int = c_int::MIN;
#[deprecated(since = "0.2.190", note = "Use `c_uint::MAX` instead.")]
pub const UINT_MAX: c_uint = c_uint::MAX;

#[deprecated(since = "0.2.190", note = "Use `c_long::MAX` instead.")]
pub const LONG_MAX: c_long = c_long::MAX;
#[deprecated(since = "0.2.190", note = "Use `c_long::MIN` instead.")]
pub const LONG_MIN: c_long = c_long::MIN;
#[deprecated(since = "0.2.190", note = "Use `c_ulong::MAX` instead.")]
pub const ULONG_MAX: c_ulong = c_ulong::MAX;

#[deprecated(since = "0.2.190", note = "Use `c_short::MAX` instead.")]
pub const SHRT_MAX: c_short = c_short::MAX;
#[deprecated(since = "0.2.190", note = "Use `c_short::MIN` instead.")]
pub const SHRT_MIN: c_short = c_short::MIN;
#[deprecated(since = "0.2.190", note = "Use `c_ushort::MAX` instead.")]
pub const USHRT_MAX: c_ushort = c_ushort::MAX;

// POSIX Limits
pub const ARG_MAX: c_int = 4096;
pub const CHILD_MAX: c_int = 25;

/// Constants may change across releases. See the [usage guidelines](crate#usage-guidelines)
/// for details.
pub const LINK_MAX: c_int = 8;

pub const MAX_CANON: c_int = 255;
pub const MAX_INPUT: c_int = 255;

/// Constants may change across releases. See the [usage guidelines](crate#usage-guidelines)
/// for details.
pub const NAME_MAX: c_int = 255;

pub const OPEN_MAX: c_int = 20;

/// Constants may change across releases. See the [usage guidelines](crate#usage-guidelines)
/// for details.
pub const PATH_MAX: c_int = 260;

pub const PIPE_BUF: c_int = 512;
pub const STREAM_MAX: c_int = 20;
pub const TZNAME_MAX: c_int = 50;

// Additional limits
pub const IOV_MAX: c_int = 16;
