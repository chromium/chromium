//! Generic versions of
//! [`TypeEq`](crate::TypeEq)/[`TypeNe`](crate::TypeNe)/[`TypeCmp`](crate::TypeCmp)
//! methods,
//! which can take any permutation of them as arguments.

mod zipping_;

/// For getting the return type of the zipping functions 
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_65")))]
pub mod zipping {

    pub use super::zipping_::{
        Zip2, Zip3, Zip4,
        Zip2Out, Zip3Out, Zip4Out,
    };
}


pub use self::zipping_::{in_array, zip2, zip3, zip4};