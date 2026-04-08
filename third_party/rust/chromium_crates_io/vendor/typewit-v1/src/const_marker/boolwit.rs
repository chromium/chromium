use core::fmt::{self, Debug};

use crate::{
    const_marker::Bool,
    TypeCmp,
    TypeEq,
    TypeWitnessTypeArg, MakeTypeWitness,
};


/// Type Witness that [`Bool<B>`](Bool) is either `Bool<true>` or `Bool<false>`.
/// 
/// Use this over [`BoolWitG`] if you have a `const B: bool` parameter already.
/// 
/// # Example
/// 
/// Making a function that takes a generic `Foo<B>` and calls methods on 
/// `Foo<false>` or `Foo<true>` depending on the value of the `const B: bool` parameter.
/// 
/// ```rust
/// use typewit::{const_marker::{Bool, BoolWit}, MakeTypeWitness};
/// 
/// 
/// assert_eq!(call_next(Incrementor::<GO_UP>(4)), Incrementor(5));
/// assert_eq!(call_next(Incrementor::<GO_UP>(5)), Incrementor(6));
/// 
/// assert_eq!(call_next(Incrementor::<GO_DOWN>(4)), Incrementor(3));
/// assert_eq!(call_next(Incrementor::<GO_DOWN>(3)), Incrementor(2));
/// 
/// 
/// const fn call_next<const B: bool>(incrementor: Incrementor<B>) -> Incrementor<B> {
///     typewit::type_fn! {
///         // type-level function from `Bool<B>` to `Incrementor<B>`
///         struct IncrementorFn;
///         impl<const B: bool> Bool<B> => Incrementor<B>
///     }
/// 
///     // The example below this one shows how to write this match more concisely
///     match BoolWit::MAKE {
///         // `bw: TypeEq<Bool<B>, Bool<true>>`
///         BoolWit::True(bw) => {
///             // `te: TypeEq<Incrementor<B>, Incrementor<true>>`
///             let te = bw.project::<IncrementorFn>();
/// 
///             // `te.to_right` casts `Incrementor<B>` to `Incrementor<true>`,
///             // (this allows calling the inherent method).
///             // 
///             // `te.to_left` casts `Incrementor<true>` to `Incrementor<B>`
///             te.to_left(te.to_right(incrementor).next())
///         }
///         // `bw: TypeEq<Bool<B>, Bool<false>>`
///         BoolWit::False(bw) => {
///             // `te: TypeEq<Incrementor<B>, Incrementor<false>>`
///             let te = bw.project::<IncrementorFn>();
/// 
///             // like the other branch, but with `Incrementor<false>`
///             te.to_left(te.to_right(incrementor).next())
///         }
///     }
/// }
/// 
/// 
/// #[derive(Debug, Copy, Clone, PartialEq, Eq)]
/// struct Incrementor<const GO_UP: bool>(usize);
/// 
/// const GO_UP: bool = true;
/// const GO_DOWN: bool = false;
/// 
/// impl Incrementor<GO_DOWN> {
///     #[track_caller]
///     pub const fn next(self) -> Self {
///         Self(self.0 - 1)
///     }
/// }
/// 
/// impl Incrementor<GO_UP> {
///     pub const fn next(self) -> Self {
///         Self(self.0 + 1)
///     }
/// }
/// 
/// ```
/// 
/// ### Using `polymatch` for conciseness
/// 
/// The [`polymatch`](crate::polymatch) macro can be used to 
/// more concisely implement the `call_next` function.
/// 
/// ```
/// # use typewit::{const_marker::{Bool, BoolWit}, MakeTypeWitness};
/// # 
/// const fn call_next<const B: bool>(incrementor: Incrementor<B>) -> Incrementor<B> {
///     typewit::type_fn! {
///         struct IncrementorFn;
///         impl<const B: bool> Bool<B> => Incrementor<B>
///     }
/// 
///     // expands to a match with two arms, 
///     // one for `BoolWit::True` and one for `BoolWit::False`,
///     // copying the expression to the right of the `=>` to both arms.
///     typewit::polymatch! {BoolWit::MAKE;
///         BoolWit::True(bw) | BoolWit::False(bw) => {
///             let te = bw.project::<IncrementorFn>();
///             te.to_left(te.to_right(incrementor).next())
///         }
///     }
/// }
/// # 
/// # #[derive(Debug, Copy, Clone, PartialEq, Eq)]
/// # struct Incrementor<const GO_UP: bool>(usize);
/// # 
/// # const GO_UP: bool = true;
/// # const GO_DOWN: bool = false;
/// # 
/// # impl Incrementor<GO_DOWN> {
/// #     #[track_caller]
/// #     pub const fn next(self) -> Self { unimplemented!() }
/// # }
/// # 
/// # impl Incrementor<GO_UP> {
/// #     pub const fn next(self) -> Self { unimplemented!() }
/// # }
/// ```
/// 
/// ### What happens without `BoolWit`
/// 
/// If the `call_next` function was defined like this:
/// ```rust,compile_fail
/// # use typewit::{const_marker::{Bool, BoolWit}, MakeTypeWitness};
/// # 
/// const fn call_next<const B: bool>(incrementor: Incrementor<B>) -> Incrementor<B> {
///     incrementor.next()
/// }
/// # #[derive(Copy, Clone)]
/// # struct Incrementor<const WRAPPING: bool>(usize);
/// # 
/// # impl Incrementor<false> {
/// #     pub const fn next(self) -> Self {
/// #         unimplemented!()
/// #     }
/// # }
/// # 
/// # impl Incrementor<true> {
/// #     pub const fn next(self) -> Self {
/// #         unimplemented!()
/// #     }
/// # }
/// ```
/// it would produce this error
/// ```text
/// error[E0599]: no method named `next` found for struct `Incrementor<B>` in the current scope
///   --> src/const_marker/const_witnesses.rs:20:17
///    |
/// 7  |     incrementor.next()
///    |                 ^^^^ method not found in `Incrementor<B>`
/// ...
/// 38 | struct Incrementor<const WRAPPING: bool>(usize);
///    | ---------------------------------------- method `next` not found for this struct
///    |
///    = note: the method was found for
///            - `Incrementor<false>`
///            - `Incrementor<true>`
/// ```
/// 
/// 
pub type BoolWit<const B: bool> = BoolWitG<Bool<B>>;


/// Type witness that `B` is either [`Bool`]`<true>` or [`Bool`]`<false>`
/// 
/// Use this over [`BoolWit`] if you want to write a [`HasTypeWitness`] bound
/// and adding a `const B: bool` parameter would be impossible.
/// 
/// # Example
/// 
/// This basic example demonstrates where `BoolWitG` would be used instead of `BoolWit`.
/// 
/// ```rust
/// use typewit::const_marker::{Bool, BoolWitG};
/// use typewit::HasTypeWitness;
/// 
/// 
/// trait Boolean: Sized + HasTypeWitness<BoolWitG<Self>> {
///     type Not: Boolean<Not = Self>;
/// }
/// 
/// impl Boolean for Bool<true> {
///     type Not = Bool<false>;
/// }
/// 
/// impl Boolean for Bool<false> {
///     type Not = Bool<true>;
/// }
/// ```
/// 
/// [`HasTypeWitness`]: crate::HasTypeWitness
pub enum BoolWitG<B> {
    /// Witnesses that `B == true`
    True(TypeEq<B, Bool<true>>),
    /// Witnesses that `B == false`
    False(TypeEq<B, Bool<false>>),
}

impl<B> BoolWitG<B> {
    /// Whether `B == Bool<true>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::BoolWitG, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).is_true(), true);
    /// assert_eq!(BoolWitG::False(TypeEq::NEW).is_true(), false);
    /// ```
    ///
    pub const fn is_true(self) -> bool {
        matches!(self, Self::True{..})
    }

    /// Whether `B == Bool<false>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::BoolWitG, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).is_false(), false);
    /// assert_eq!(BoolWitG::False(TypeEq::NEW).is_false(), true);
    /// ```
    ///
    pub const fn is_false(self) -> bool {
        matches!(self, Self::False{..})
    }

    /// Gets a proof of `B == Bool<true>`, returns None if `B == Bool<false>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).to_true(), Some(TypeEq::new::<Bool<true>>()));
    /// assert_eq!(BoolWitG::False(TypeEq::NEW).to_true(), None);
    /// ```
    ///
    pub const fn to_true(self) -> Option<TypeEq<B, Bool<true>>> {
        match self {
            Self::True(x) => Some(x),
            Self::False{..} => None
        }
    }

    /// Gets a proof of `B == Bool<false>`, returns None if `B == Bool<true>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).to_false(), None);
    /// assert_eq!(BoolWitG::False(TypeEq::NEW).to_false(), Some(TypeEq::new::<Bool<false>>()));
    /// ```
    ///
    pub const fn to_false(self) -> Option<TypeEq<B, Bool<false>>> {
        match self {
            Self::False(x) => Some(x),
            Self::True{..} => None
        }
    }


    /// Gets a proof of `B == Bool<true>`.
    ///
    /// # Panic
    ///
    /// Panics if `B == Bool<false>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).unwrap_true(), TypeEq::new::<Bool<true>>());
    /// ```
    ///
    pub const fn unwrap_true(self) -> TypeEq<B, Bool<true>> {
        match self {
            Self::True(x) => x,
            Self::False{..}  => panic!("attempted to unwrap into True on False variant")
        }
    }

    /// Gets a proof of `B == Bool<true>`.
    ///
    /// # Panic
    ///
    /// Panics if `B == Bool<false>`, with `msg` as the panic message.
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::True(TypeEq::NEW).expect_true(":("), TypeEq::new::<Bool<true>>());
    /// ```
    ///
    pub const fn expect_true(self, msg: &str) -> TypeEq<B, Bool<true>> {
        match self {
            Self::True(x) => x,
            Self::False{..}  => panic!("{}", msg)
        }
    }

    /// Gets a proof of `B == Bool<false>`.
    ///
    /// # Panic
    /// 
    /// Panics if `B == Bool<true>`
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(BoolWitG::False(TypeEq::NEW).unwrap_false(), TypeEq::new::<Bool<false>>());
    /// ```
    ///
    pub const fn unwrap_false(self) -> TypeEq<B, Bool<false>> {
        match self {
            Self::False(x) => x,
            Self::True{..} => panic!("attempted to unwrap into False on True variant")
        }
    }

    /// Gets a proof of `B == Bool<false>`.
    ///
    /// # Panic
    /// 
    /// Panics if `B == Bool<true>`, with `msg` as the panic message.
    ///
    /// # Example
    ///
    /// ```rust
    /// use typewit::{const_marker::{Bool, BoolWitG}, TypeEq};
    /// 
    /// assert_eq!(
    ///     BoolWitG::False(TypeEq::NEW).expect_false("it is false"),
    ///     TypeEq::new::<Bool<false>>(),
    /// );
    /// ```
    ///
    pub const fn expect_false(self, msg: &str) -> TypeEq<B, Bool<false>> {
        match self {
            Self::False(x) => x,
            Self::True{..} => panic!("{}", msg)
        }
    }

}

impl<B> Copy for BoolWitG<B> {}

impl<B> Clone for BoolWitG<B> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<B> Debug for BoolWitG<B> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match self {
            Self::True{..} => "True",
            Self::False{..} => "False",
        })
    }
}

impl<B> TypeWitnessTypeArg for BoolWitG<B> {
    type Arg = B;
}

impl<const B: bool> MakeTypeWitness for BoolWitG<Bool<B>> {
    const MAKE: Self = {
        if let TypeCmp::Eq(te) = Bool.equals(Bool) {
            BoolWit::True(te)
        } else if let TypeCmp::Eq(te) = Bool.equals(Bool) {
            BoolWit::False(te)
        } else {
            panic!("unreachable: `B` is either `true` or `false`")
        }
    };
}

