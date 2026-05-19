#![allow(non_snake_case)] // non-snake case identifiers used in define_tuple_list_traits! for simplicity
#![doc(html_playground_url = "https://play.rust-lang.org/")]
#![cfg_attr(not(feature = "std"), no_std)]

//! Crate for macro-free variadic tuple metaprogramming.
//! 
//! # Rationale
//! 
//! As of writing this crate, Rust does not support variadic generics
//! and does not allow to reason about tuples in general.
//! 
//! Most importantly, Rust does not allow one to generically
//! implement a trait for all tuples whose elements implement it.
//! 
//! This crate attempts to fill the gap by providing a way
//! to recursively define traits for tuples.
//! 
//! # Tuple lists
//! 
//! Tuple `(A, B, C, D)` can be unambiguously mapped into recursive tuple `(A, (B, (C, (D, ()))))`.
//! 
//! On each level it consists of a pair `(Head, Tail)`, where `Head` is tuple element and
//! `Tail` is a remainder of the list. For last element `Tail` is an empty list.
//! 
//! Unlike regular flat tuples, such recursive tuples can be effectively reasoned about in Rust.
//! 
//! This crate calls such structures "tuple lists" and provides a set of traits and macros
//! allowing one to conveniently work with them.
//! 
//! # Example 1: `PlusOne` recursive trait
//! 
//! Let's create a trait which adds one to each element of a tuple list
//! of arbitrary length, behaving differently depending on element type.
//! 
//! ```
//! // `TupleList` is a helper trait implemented by all tuple lists.
//! // Its use is optional, but it allows to avoid accidentally
//! // implementing traits for something other than tuple lists.
//! use tuple_list::TupleList;
//! 
//! // Define trait and implement it for several primitive types.
//! trait PlusOne {
//!     fn plus_one(&mut self);
//! }
//! impl PlusOne for i32    { fn plus_one(&mut self) { *self += 1; } }
//! impl PlusOne for bool   { fn plus_one(&mut self) { *self = !*self; } }
//! impl PlusOne for String { fn plus_one(&mut self) { self.push('1'); } }
//! 
//! // Now we have to implement trait for an empty tuple,
//! // thus defining initial condition.
//! impl PlusOne for () {
//!     fn plus_one(&mut self) {}
//! }
//! 
//! // Now we can implement trait for a non-empty tuple list,
//! // thus defining recursion and supporting tuple lists of arbitrary length.
//! impl<Head, Tail> PlusOne for (Head, Tail) where
//!     Head: PlusOne,
//!     Tail: PlusOne + TupleList,
//! {
//!     fn plus_one(&mut self) {
//!         self.0.plus_one();
//!         self.1.plus_one();
//!     }
//! }
//! 
//! // `tuple_list!` as a helper macro used to create
//! // tuple lists from a list of arguments.
//! use tuple_list::tuple_list;
//! 
//! // Now we can use our trait on tuple lists.
//! let mut tuple_list = tuple_list!(2, false, String::from("abc"));
//! tuple_list.plus_one();
//! 
//! // `tuple_list!` macro also allows us to unpack tuple lists
//! let tuple_list!(a, b, c) = tuple_list;
//! assert_eq!(a, 3);
//! assert_eq!(b, true);
//! assert_eq!(&c, "abc1");
//! ```
//! 
//! # Example 2: `CustomDisplay` recursive trait
//! 
//! Let's create a simple `Display`-like trait implemented for all tuples
//! lists whose elements implement it.
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::tuple_list;
//! // Define the trait and implement it for several standard types.
//! trait CustomDisplay {
//!     fn fmt(&self) -> String;
//! }
//! impl CustomDisplay for i32  { fn fmt(&self) -> String { self.to_string() } }
//! impl CustomDisplay for bool { fn fmt(&self) -> String { self.to_string() } }
//! impl CustomDisplay for &str { fn fmt(&self) -> String { self.to_string() } }
//! 
//! // Now we have to implement trait for an empty tuple,
//! // thus defining initial condition.
//! impl CustomDisplay for () {
//!     fn fmt(&self) -> String { String::from("<empty>") }
//! }
//! 
//! // In order to avoid having trailing spaces, we need
//! // custom logic for tuple lists of exactly one element.
//! //
//! // The simplest way is to use `TupleList::TUPLE_LIST_SIZE`
//! // associated constant, but there is also another option.
//! //
//! // Instead of defining initial condition for empty tuple list
//! // and recursion for non-empty ones, we can define *two*
//! // initial conditions: one for an empty tuple list and
//! // one for tuple lists of exactly one element.
//! // Then we can define recursion for tuple lists of two or more elements.
//! //
//! // Here we define second initial condition for tuple list
//! // of exactly one element.
//! impl<Head> CustomDisplay for (Head, ()) where
//!     Head: CustomDisplay,
//! {
//!     fn fmt(&self) -> String {
//!         return self.0.fmt()
//!     }
//! }
//! 
//! // Recursion step is defined for all tuple lists
//! // longer than one element.
//! impl<Head, Next, Tail> CustomDisplay for (Head, (Next, Tail)) where
//!     Head: CustomDisplay,
//!     (Next, Tail): CustomDisplay + TupleList,
//!     Tail: TupleList,
//! {
//!     fn fmt(&self) -> String {
//!         return format!("{} {}", self.0.fmt(), self.1.fmt());
//!     }
//! }
//! 
//! // Ensure `fmt` is called for each element.
//! let tuple_list = tuple_list!(2, false, "abc");
//! assert_eq!(
//!     tuple_list.fmt(),
//!     "2 false abc",
//! );
//! 
//! // Since tuple lists implement `CustomDisplay` themselves, they can
//! // be elements in other tuple lists implementing `CustomDisplay`.
//! let nested_tuple_list = tuple_list!(2, false, "abc", tuple_list!(3, true, "def"));
//! assert_eq!(
//!     nested_tuple_list.fmt(),
//!     "2 false abc 3 true def",
//! );
//! ```
//! 
//! # Example 3: `SwapStringAndInt` recursive trait
//! 
//! Let's implement a trait which converts `i32` to `String` and vice versa.
//! 
//! This example is way more complex that the other
//! because it maps one tuple list into another tuple list.
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::tuple_list;
//! // Let's define and implement a trait for `i32` and `String`
//! // so that it converts `String` to `i32` and vice versa.
//! trait SwapStringAndInt {
//!     type Other;
//!     fn swap(self) -> Self::Other;
//! }
//! impl SwapStringAndInt for i32 {
//!     type Other = String;
//!     fn swap(self) -> String { self.to_string() }
//! }
//! impl SwapStringAndInt for String {
//!     type Other = i32;
//!     fn swap(self) -> i32 { self.parse().unwrap() }
//! }
//! 
//! // Now we have to implement trait for an empty tuple,
//! // thus defining initial condition.
//! impl SwapStringAndInt for () {
//!     type Other = ();
//!     fn swap(self) -> () { () }
//! }
//! 
//! // Now we can implement trait for a non-empty tuple list,
//! // thus defining recursion and supporting tuple lists of arbitrary length.
//! impl<Head, Tail, TailOther> SwapStringAndInt for (Head, Tail) where
//!     Head: SwapStringAndInt,
//!     Tail: SwapStringAndInt<Other=TailOther> + TupleList,
//!     TailOther: TupleList,
//! {
//!     type Other = (Head::Other, Tail::Other);
//!     fn swap(self) -> Self::Other {
//!         (self.0.swap(), self.1.swap())
//!     }
//! }
//! 
//! // Tuple lists implement `SwapStringAndInt` by calling `SwapStringAndInt::swap`
//! // on each member and returning tuple list of resulting values.
//! let original = tuple_list!(4, String::from("2"), 7, String::from("13"));
//! let swapped  = tuple_list!(String::from("4"), 2, String::from("7"), 13);
//! 
//! assert_eq!(original.swap(), swapped);
//! ```
//! 
//! # Example 4: prepend and append functions
//! 
//! Let's implement append and prepend functions for tuple lists.
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::tuple_list;
//! // Prepend is a trivial operation for tuple lists.
//! // We just create a new pair from prepended element
//! // and the remainder of the list.
//! fn prepend<T, Tail: TupleList>(value: T, tail: Tail) -> (T, Tail) {
//!     (value, tail)
//! }
//! 
//! // Append is a bit more comples. We'll need a trait for that.
//! trait Append<T>: TupleList {
//!     type AppendResult: TupleList;
//! 
//!     fn append(self, value: T) -> Self::AppendResult;
//! }
//! 
//! // Implement append for an empty tuple list.
//! impl<T> Append<T> for () {
//!     type AppendResult = (T, ());
//! 
//!     // Append for an empty tuple list is quite trivial.
//!     fn append(self, value: T) -> Self::AppendResult { (value, ()) }
//! }
//! 
//! // Implement append for non-empty tuple list.
//! impl<Head, Tail, T> Append<T> for (Head, Tail) where
//!     Self: TupleList,
//!     Tail: Append<T>,
//!     (Head, Tail::AppendResult): TupleList,
//! {
//!     type AppendResult = (Head, Tail::AppendResult);
//! 
//!     // Here we deconstruct tuple list,
//!     // recursively call append on the
//!     // tail of it, and then reconstruct
//!     // the list using the new tail.
//!     fn append(self, value: T) -> Self::AppendResult {
//!         let (head, tail) = self;
//!         return (head, tail.append(value));
//!     }
//! }
//! 
//! // Now we can use our append and prepend functions
//! // on tuple lists.
//! let original  = tuple_list!(   1, "foo", false);
//! let appended  = tuple_list!(   1, "foo", false, 5);
//! let prepended = tuple_list!(5, 1, "foo", false);
//! 
//! assert_eq!(original.append(5), appended);
//! assert_eq!(prepend(5, original), prepended);
//! ```
//! 
//! # Example 5: reverse function
//! 
//! We can also implement a function which reverses elements of a tuple list.
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::tuple_list;
//! // Rewind is a helper trait which maintains two tuple lists:
//! // `Todo` (which is `Self` for the trait) is the remainder of a tuple list to be reversed.
//! // `Done` is already reversed part of it.
//! trait Rewind<Done: TupleList> {
//!     // RewindResult is the type of fully reversed tuple.
//!     type RewindResult: TupleList;
//! 
//!     fn rewind(self, done: Done) -> Self::RewindResult;
//! }
//! 
//! // Initial condition.
//! impl<Done: TupleList> Rewind<Done> for () {
//!     type RewindResult = Done;
//! 
//!     // When nothing is left to do, just return reversed tuple list.
//!     fn rewind(self, done: Done) -> Done { done }
//! }
//! 
//! // Recursion step.
//! impl<Done, Next, Tail> Rewind<Done> for (Next, Tail) where
//!     Done: TupleList,
//!     (Next, Done): TupleList,
//!     Tail: Rewind<(Next, Done)> + TupleList,
//! {
//!     type RewindResult = Tail::RewindResult;
//! 
//!     // Strip head element from `Todo` and prepend it to `Done` list,
//!     // then recurse on remaining tail of `Todo`.
//!     fn rewind(self, done: Done) -> Self::RewindResult {
//!         let (next, tail) = self;
//!         return tail.rewind((next, done));
//!     }
//! }
//! 
//! // Helper function which uses `Rewind` trait to reverse a tuple list.
//! fn reverse<T>(tuple: T) -> T::RewindResult where
//!     T: Rewind<()>
//! {
//!     // Initial condition, whole tuple list is `Todo`,
//!     // empty tuple is `Done`.
//!     tuple.rewind(())
//! }
//! 
//! // Now `reverse` is usable on tuple lists.
//! let original = tuple_list!(1, "foo", false);
//! let reversed = tuple_list!(false, "foo", 1);
//! 
//! assert_eq!(reverse(original), reversed);
//! ```
//! 
//! # Tuple lists and tuples interoperability
//! 
//! This crate defines `Tuple` and `TupleList` traits, which
//! are automatically implemented and allow you to convert
//! tuples into tuple lists and vice versa.
//! 
//! Best way to handle interoperability is to store your data
//! as tuple lists and convert them to tuples if necessary.
//! 
//! Alternatively it's possible to create a helper function
//! which accepts a tuple, converts it to a tuple list,
//! calls trait method and then returns the result.
//! 
//! Here's an example of such function for `Append`
//! trait from previous example:
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::tuple_list;
//! # trait Append<T>: TupleList {
//! #     type AppendResult: TupleList;
//! #     fn append(self, value: T) -> Self::AppendResult;
//! # }
//! # impl<T> Append<T> for () {
//! #     type AppendResult = (T, ());
//! #     fn append(self, value: T) -> Self::AppendResult { (value, ()) }
//! # }
//! # impl<Head, Tail, T> Append<T> for (Head, Tail) where
//! #     Self: TupleList,
//! #     Tail: Append<T>,
//! #     (Head, Tail::AppendResult): TupleList,
//! # {
//! #     type AppendResult = (Head, Tail::AppendResult);
//! #     fn append(self, value: T) -> Self::AppendResult {
//! #         let (head, tail) = self;
//! #         return (head, tail.append(value));
//! #     }
//! # }
//! // `Tuple` trait is needed to access conversion function.
//! use tuple_list::Tuple;
//! 
//! fn append<T, AppendedTupleList, Elem>(tuple: T, elem: Elem) -> AppendedTupleList::Tuple where
//!     T: Tuple,                                                   // input argument tuple
//!     T::TupleList: Append<Elem, AppendResult=AppendedTupleList>, // input argument tuple list
//!     AppendedTupleList: TupleList,                               // resulting tuple list
//! {
//!     // Convert tuple into tuple list, append the element
//!     // and convert the result back into tuple.
//!     tuple.into_tuple_list().append(elem).into_tuple()
//! }
//! 
//! // Unlike `Append` trait which is defined for tuple lists,
//! // `append` function works on regular tuples.
//! let original = (1, "foo", false);
//! let appended = (1, "foo", false, 5);
//! 
//! assert_eq!(append(original, 5), appended);
//! ```
//! 
//! Please note that tuple/tuple list conversions are
//! destructive and consume the original, which seemingly
//! prevents you from, for example, modifying content
//! of the original tuple.
//! 
//! In order to alleviate this problem, `tuple_list` crate
//! introduces `AsTupleOfRefs` trait, which allows one to
//! convert reference to tuple into tuple of references.
//! 
//! The idea is that if you you can convert reference to tuple
//! into tuple of references, then convert tuple of references
//! into tuple list and then use recursive trait as usual.
//! 
//! Let's modify `PlusOne` trait example so it can be used
//! to modify regular tuples:
//! 
//! ```
//! # use tuple_list::TupleList;
//! # use tuple_list::Tuple;
//! # use tuple_list::AsTupleOfRefs;
//! // Define trait and implement it for several primitive types.
//! trait PlusOne {
//!     fn plus_one(&mut self);
//! }
//! impl PlusOne for i32    { fn plus_one(&mut self) { *self += 1; } }
//! impl PlusOne for bool   { fn plus_one(&mut self) { *self = !*self; } }
//! impl PlusOne for String { fn plus_one(&mut self) { self.push('1'); } }
//! 
//! // Now we have to define a new trait
//! // specifically for tuple lists of references.
//! //
//! // Unlike the original, it accepts `self` by value.
//! trait PlusOneTupleList: TupleList {
//!     fn plus_one(self);
//! }
//! 
//! // Now we have to implement trait for an empty tuple,
//! // thus defining initial condition.
//! impl PlusOneTupleList for () {
//!     fn plus_one(self) {}
//! }
//! 
//! // Now we can implement trait for a non-empty tuple list,
//! // thus defining recursion and supporting tuple lists of arbitrary length.
//! //
//! // Note that we're implementing `PlusOneTupleList` for
//! // *tuple list of mutable references*, and as a result
//! // head of the list is a mutable reference, not a value.
//! impl<Head, Tail> PlusOneTupleList for (&mut Head, Tail) where
//!     Self: TupleList,
//!     Head: PlusOne,
//!     Tail: PlusOneTupleList,
//! {
//!     fn plus_one(self) {
//!         self.0.plus_one();
//!         self.1.plus_one();
//!     }
//! }
//! 
//! // Now let's define a helper function operating on regular tuples.
//! fn plus_one<'a, T, RT>(tuple: &'a mut T) where
//!     T: AsTupleOfRefs<'a, TupleOfMutRefs=RT>,
//!     RT: Tuple + 'a,
//!     RT::TupleList: PlusOneTupleList,
//! 
//! {
//!     tuple.as_tuple_of_mut_refs().into_tuple_list().plus_one()
//! }
//! 
//! // Now we can use this helper function on regular tuples.
//! let mut tuple = (2, false, String::from("abc"));
//! plus_one(&mut tuple);
//! 
//! assert_eq!(tuple.0, 3);
//! assert_eq!(tuple.1, true);
//! assert_eq!(&tuple.2, "abc1");
//! ```
//! 
//! As you can see, working with tuples requires a lot
//! of bolierplate code. Unless you have preexisting code
//! you need to support, it's generally better to use
//! tuple lists directly, since they are much easier
//! to work with.
//! 
//! # Implementing recursive traits for regular tuples
//! 
//! Implementing recursive traits for regular tuples poses
//! certain problems. As of now it is possible within
//! `tuple_list` crate, but quickly leads to orphan rules
//! violations when used outside of it.
//! 
//! You can see a working example of a trait implemented for
//! regular tuples in `tuple_list::test::all_features`,
//! but it's overly complex and pretty much experimental.
//! 
//! It should be possible to define recursive traits on regular tuples
//! once trait specialization feature is implemented in Rust.

/// Trait providing conversion from tuple list into tuple.
///
/// Generic trait implemented for all tuple lists (up to 12 elements).
/// 
/// # Examples
/// 
/// ```
/// use crate::tuple_list::tuple_list;
/// use crate::tuple_list::TupleList;
/// 
/// let tuple_list = tuple_list!(1, false, "abc");
/// 
/// assert_eq!(
///     tuple_list.into_tuple(),
///     (1, false, "abc"),
/// );
/// ```
pub trait TupleList where Self: Sized {
    /// Tuple type corresponding to given tuple list.
    type Tuple: Tuple<TupleList=Self>;

    /// Constant representing tuple list size.
    const TUPLE_LIST_SIZE: usize;

    /// Converts tuple list into tuple.
    fn into_tuple(self) -> Self::Tuple;
}

/// Trait providing conversion from tuple into tuple list.
/// 
/// Generic trait implemented for all tuples (up to 12 elements).
/// 
/// Please note that `Tuple` trait does not have
/// `TUPLE_SIZE` constant like `TupleList` does.
/// 
/// This is intentional, in order to avoid accidental use of it for tuple lists.
/// 
/// You can still get tuple size as `Tuple::TupleList::TUPLE_LIST_SIZE`.
/// 
/// # Examples
/// 
/// ```
/// use crate::tuple_list::Tuple;
/// 
/// let tuple = (1, false, "abc");
/// 
/// assert_eq!(
///     tuple.into_tuple_list(),
///     (1, (false, ("abc", ()))),
/// );
/// ```
pub trait Tuple where Self: Sized {
    /// Tuple list type corresponding to given tuple.
    type TupleList: TupleList<Tuple=Self>;

    /// Converts tuple into tuple list.
    fn into_tuple_list(self) -> Self::TupleList;
}

/// Trait providing conversion from references to tuples into tuples of references.
/// 
/// Generic trait implemented for all tuples (up to 12 elements).
/// 
/// # Example
/// ```
/// use tuple_list::AsTupleOfRefs;
/// 
/// fn by_val(tuple: (i32, i32)) {}
/// fn by_ref(tuple: (&i32, &i32)) {}
/// fn by_mut(tuple: (&mut i32, &mut i32)) {}
/// 
/// let mut tuple = (1, 2);
/// by_val(tuple);
/// by_ref(tuple.as_tuple_of_refs());
/// by_mut(tuple.as_tuple_of_mut_refs());
/// ```
// TODO: when rust gets generic associated types
//       move this trait content into Tuple
pub trait AsTupleOfRefs<'a>: Tuple {
    type TupleOfRefs: Tuple + 'a;
    type TupleOfMutRefs: Tuple + 'a;

    /// Converts reference to tuple into tuple of references.
    fn as_tuple_of_refs(&'a self) -> Self::TupleOfRefs;

    /// Converts mutable reference to tuple into tuple of mutable references.
    fn as_tuple_of_mut_refs(&'a mut self) -> Self::TupleOfMutRefs;
}

/// Trait providing tuple construction function, allows to prepend a value to a tuple.
// TODO: when rust gets generic associated types
//       move this trait content into Tuple
pub trait TupleCons<Head>: Tuple {
    /// Tuple with `Head` prepended to `Self`
    type ConsResult: Tuple;

    /// Constructs a tuple from `head` value and `tail` tuple by prepending `head` to `tail`.
    /// 
    /// Reverse of `NonEmptyTuple::uncons`.
    /// 
    /// # Examples
    /// 
    /// ```
    /// use tuple_list::TupleCons;
    /// 
    /// let a = TupleCons::cons("foo", ());
    /// assert_eq!(
    ///     a,
    ///     ("foo",),
    /// );
    /// 
    /// let b = TupleCons::cons(false, a);
    /// assert_eq!(
    ///     b,
    ///     (false, "foo"),
    /// );
    /// 
    /// let c = TupleCons::cons(4, b);
    /// assert_eq!(
    ///     c,
    ///     (4, false, "foo"),
    /// );
    /// ```
    fn cons(head: Head, tail: Self) -> Self::ConsResult;
}

/// Trait allowing to recursively deconstruct tuples.
/// 
/// Generic trait implemented for all non-empty tuples (up to 12 elements).
/// 
/// Most interesting part is that this trait allows you to recursively
/// define some simple traits for regular tuples.
/// 
/// Unofrtunately, it's not quite complete and is pretty unusable as of now.
/// 
/// In order ot be usable outside of this crate it needs support
/// for trait specializations in Rust.
pub trait NonEmptyTuple: Tuple {
    /// First element of `Self` tuple.
    type Head;
    /// Tuple of remaining elements of `Self` tuple.
    type Tail: Tuple;

    /// Splits `Self` tuple into head value and tail tuple.
    /// 
    /// Reverse of `TupleCons::cons`.
    /// 
    /// # Examples
    /// 
    /// ```
    /// use tuple_list::NonEmptyTuple;
    /// 
    /// let abcz = (4, false, "foo");
    /// 
    /// let (a, bcz) = NonEmptyTuple::uncons(abcz);
    /// assert_eq!(a, 4);
    /// assert_eq!(bcz, (false, "foo"));
    /// 
    /// let (b, cz) = NonEmptyTuple::uncons(bcz);
    /// assert_eq!(b, false);
    /// assert_eq!(cz, ("foo",));
    /// 
    /// let (c, z)  = NonEmptyTuple::uncons(cz);
    /// assert_eq!(c, "foo");
    /// assert_eq!(z, ());
    /// ```
    fn uncons(self) -> (Self::Head, Self::Tail);

    /// Returns first element of a tuple.
    /// 
    /// Same as `NonEmptyTuple::uncons().0`.
    fn head(self) -> Self::Head;

    /// Returns all but the first element of a tuple.
    /// 
    /// Same as `NonEmptyTuple::uncons().1`.
    fn tail(self) -> Self::Tail;
}

/// Macro creating tuple list values from list of expressions.
/// 
/// # Examples
/// 
/// Main use of this macro is to create tuple list values:
/// 
/// ```
/// use tuple_list::tuple_list;
/// 
/// let list = tuple_list!(10, false, "foo");
/// 
/// assert_eq!(
///   list,
///   (10, (false, ("foo", ()))),
/// )
/// ```
/// 
/// Aside from that, `tuple_list!` can sometime be used to define trivial types,
/// but using macro `tuple_list_type!` is recommended instead:
/// 
/// ```
/// # use tuple_list::tuple_list;
/// # use std::collections::HashMap;
/// // Trivial types work just fine with `tuple_list!`.
/// let list: tuple_list_type!(i32, bool, String) = Default::default();
/// 
/// // More complex types will fail when using `tuple_list!`,
/// // but will work with `tuple_list_type!`.
/// use tuple_list::tuple_list_type;
/// 
/// let list: tuple_list_type!(
///     &'static str,
///     HashMap<i32, i32>,
///     <std::vec::Vec<bool> as IntoIterator>::Item,
/// ) = tuple_list!("foo", HashMap::new(), false);
/// ```
/// 
/// It can also be used to unpack tuples:
/// 
/// ```
/// # use tuple_list::tuple_list;
/// let tuple_list!(a, b, c) = tuple_list!(10, false, "foo");
/// 
/// assert_eq!(a, 10);
/// assert_eq!(b, false);
/// assert_eq!(c, "foo");
/// ```
/// 
/// Unfortunately, due to Rust macro limitations only simple, non-nested match patterns are supported.
#[macro_export]
macro_rules! tuple_list {
    () => ( () );

    // handling simple identifiers, for limited types and patterns support
    ($i:ident)  => ( ($i, ()) );
    ($i:ident,) => ( ($i, ()) );
    ($i:ident, $($e:ident),*)  => ( ($i, $crate::tuple_list!($($e),*)) );
    ($i:ident, $($e:ident),*,) => ( ($i, $crate::tuple_list!($($e),*)) );

    // handling complex expressions
    ($i:expr)  => ( ($i, ()) );
    ($i:expr,) => ( ($i, ()) );
    ($i:expr, $($e:expr),*)  => ( ($i, $crate::tuple_list!($($e),*)) );
    ($i:expr, $($e:expr),*,) => ( ($i, $crate::tuple_list!($($e),*)) );
}

/// Macro creating tuple list types from list of element types.
/// 
/// See macro `tuple_list!` for details.
#[macro_export]
macro_rules! tuple_list_type {
    () => ( () );

    ($i:ty)  => ( ($i, ()) );
    ($i:ty,) => ( ($i, ()) );
    ($i:ty, $($e:ty),*)  => ( ($i, $crate::tuple_list_type!($($e),*)) );
    ($i:ty, $($e:ty),*,) => ( ($i, $crate::tuple_list_type!($($e),*)) );
}

// helper, returns first argument, ignores the rest
macro_rules! list_head {
    ($i:ident) => ( $i );
    ($i:ident, $($e:ident),+) => ( $i );
}

// helper, returns all arguments but the first one
macro_rules! list_tail {
    ($i:ident) => ( () );
    ($i:ident, $e:ident) => ( ($e,) );
    ($i:ident, $($e:ident),+) => ( ($($e),*,) );
}

// defines Tuple, TupleList, TupleCons, NonEmptyTuple and AsTupleOfRefs
macro_rules! define_tuple_list_traits {
    () => (
        impl TupleList for () {
            type Tuple = ();
            const TUPLE_LIST_SIZE: usize = 0;
            fn into_tuple(self) {}
        }
        impl Tuple for () {
            type TupleList = ();
            fn into_tuple_list(self) -> () { () }
        }
        impl<'a> AsTupleOfRefs<'a> for () {
            type TupleOfRefs = ();
            type TupleOfMutRefs = ();
            fn as_tuple_of_refs(&'a self) {}
            fn as_tuple_of_mut_refs(&'a mut self) {}
        }
    );
    ($($x:ident),*) => (
        impl<$($x),*> TupleList for tuple_list_type!($($x),*) {
            type Tuple = ($($x),*,);
            const TUPLE_LIST_SIZE: usize = <list_tail!($($x),*) as Tuple>::TupleList::TUPLE_LIST_SIZE + 1;
            fn into_tuple(self) -> Self::Tuple {
                let tuple_list!($($x),*) = self;
                return ($($x),*,)
            }
        }
        impl<$($x),*> Tuple for ($($x),*,) {
            type TupleList = tuple_list_type!($($x),*);
            fn into_tuple_list(self) -> Self::TupleList {
                let ($($x),*,) = self;
                return tuple_list!($($x),*);
            }
        }
        impl<'a, $($x: 'a),*> AsTupleOfRefs<'a> for ($($x),*,) {
            type TupleOfRefs = ($(&'a $x),*,);
            type TupleOfMutRefs = ($(&'a mut $x),*,);
            fn as_tuple_of_refs(&'a self) -> Self::TupleOfRefs {
                let ($($x),*,) = self;
                return ($($x),*,);
            }
            fn as_tuple_of_mut_refs(&'a mut self) -> Self::TupleOfMutRefs {
                let ($($x),*,) = self;
                return ($($x),*,);
            }
        }
        impl<$($x),*> NonEmptyTuple for ($($x),*,) {
            type Head = list_head!($($x),*);
            type Tail = list_tail!($($x),*);
            fn uncons(self) -> (Self::Head, Self::Tail) {
                let ($($x),*,) = self;
                return (list_head!($($x),*), list_tail!($($x),*));
            }
            fn head(self) -> Self::Head { self.0 }
            fn tail(self) -> Self::Tail { self.uncons().1 }
        }
        impl<$($x),*> TupleCons<list_head!($($x),*)> for list_tail!($($x),*) {
            type ConsResult = ($($x),*,);
            fn cons(head: list_head!($($x),*), tail: Self) -> Self::ConsResult {
                let list_head!($($x),*) = head;
                let list_tail!($($x),*) = tail;
                return ($($x),*,);
            }
        }
    );
}

// rust only defines common traits for tuples up to 12 elements
// we'll do the same here, increase number if needed
define_tuple_list_traits!();
define_tuple_list_traits!(T1);
define_tuple_list_traits!(T1, T2);
define_tuple_list_traits!(T1, T2, T3);
define_tuple_list_traits!(T1, T2, T3, T4);
define_tuple_list_traits!(T1, T2, T3, T4, T5);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7, T8);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7, T8, T9);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11);
define_tuple_list_traits!(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12);

#[cfg(test)]
mod tests;
