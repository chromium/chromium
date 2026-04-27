//! Machinery for making the comparison macros work with both standard and user-defined types.
//!
//! To make those macros work with both std and non-std types,
//! they must implement [`ConstCmpMarker`] to describe whether they're from std or not.
//!
//!
//! [`ConstCmpMarker`]: trait.ConstCmpMarker.html

mod cmp_wrapper;
mod const_cmp_marker;

pub use self::{
    cmp_wrapper::CmpWrapper,
    const_cmp_marker::{ConstCmpMarker, IsAConstCmpMarker, IsArrayKind, IsNotStdKind, IsStdKind},
};
