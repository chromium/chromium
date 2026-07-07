//! Less used details of `CxxVector`.
//!
//! `CxxVector` itself is exposed at the crate root.

#[doc(inline)]
pub use crate::Vector;
pub use crate::cxx_vector::{Iter, IterMut, VectorElement};
#[doc(no_inline)]
pub use cxx::CxxVector;
