use crate::prelude::*;

pub type c_long = i64;
pub type c_ulong = u64;
pub type c_char = u8;

pub(crate) const _ALIGNBYTES: usize = mem::size_of::<c_long>() - 1;

pub const _MAX_PAGE_SHIFT: u32 = 12;
