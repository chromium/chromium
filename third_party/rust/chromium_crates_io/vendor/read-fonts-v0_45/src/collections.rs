//! Data structures useful for font work.

pub mod int_set;
pub use int_set::IntSet;
pub use int_set::U32Set;

mod range_set;
pub use range_set::RangeSet;

#[cfg(feature = "std")]
pub(crate) mod fnv;
#[cfg(feature = "std")]
pub(crate) use fnv::FnvHashMap;
