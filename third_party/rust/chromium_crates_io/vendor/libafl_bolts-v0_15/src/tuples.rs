//! Compiletime lists/tuples used throughout the `LibAFL` universe

#[cfg(feature = "alloc")]
use alloc::{borrow::Cow, vec::Vec};
use core::{any::TypeId, mem::transmute};
#[cfg(feature = "alloc")]
use core::{
    any::type_name,
    fmt::{Debug, Formatter},
    marker::PhantomData,
    ops::{Deref, DerefMut, Index, IndexMut},
};

#[cfg(feature = "alloc")]
use serde::{Deserialize, Serialize};
pub use tuple_list::{TupleList, tuple_list, tuple_list_type};

use crate::HasLen;
#[cfg(feature = "alloc")]
use crate::Named;
#[cfg(any(feature = "xxh3", feature = "alloc"))]
use crate::hash_std;
#[cfg(feature = "alloc")]
use crate::tuples::seal::{InnerBorrowMut, StackedExtract};

/// Returns if the type `T` is equal to `U`, ignoring lifetimes.
#[must_use]
pub fn type_eq<T: ?Sized, U: ?Sized>() -> bool {
    typeid::of::<T>() == typeid::of::<U>()
}

/// Borrow each member of the tuple
pub trait SplitBorrow<'a> {
    /// The Resulting [`TupleList`], of an [`SplitBorrow::borrow()`] call
    type SplitBorrowResult;
    /// The Resulting [`TupleList`], of an [`SplitBorrow::borrow_mut()`] call
    type SplitBorrowMutResult;

    /// Return a tuple of borrowed references
    fn borrow(&'a self) -> Self::SplitBorrowResult;
    /// Return a tuple of borrowed mutable references
    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult;
}

impl<'a> SplitBorrow<'a> for () {
    type SplitBorrowResult = ();
    type SplitBorrowMutResult = ();

    fn borrow(&'a self) -> Self::SplitBorrowResult {}

    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult {}
}

impl<'a, Head, Tail> SplitBorrow<'a> for (Head, Tail)
where
    Head: 'a,
    Tail: SplitBorrow<'a>,
{
    type SplitBorrowResult = (Option<&'a Head>, Tail::SplitBorrowResult);
    type SplitBorrowMutResult = (Option<&'a mut Head>, Tail::SplitBorrowMutResult);

    fn borrow(&'a self) -> Self::SplitBorrowResult {
        (Some(&self.0), self.1.borrow())
    }

    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult {
        (Some(&mut self.0), self.1.borrow_mut())
    }
}

/// Create a [`Vec`] from a tuple list or similar
/// (We need this trait since we cannot implement `Into` for foreign types)
#[cfg(feature = "alloc")]
pub trait IntoVec<T> {
    /// Convert this into a [`Vec`], reversed.
    /// (Having this method around makes some implementations more performant)
    fn into_vec_reversed(self) -> Vec<T>
    where
        Self: Sized,
    {
        let mut ret = self.into_vec();
        ret.reverse();
        ret
    }

    /// Convert this into a [`Vec`].
    fn into_vec(self) -> Vec<T>;
}

#[cfg(feature = "alloc")]
impl<T> IntoVec<T> for () {
    #[inline]
    fn into_vec(self) -> Vec<T> {
        Vec::new()
    }
}

/// Gets the length of the element
pub trait HasConstLen {
    /// The length as constant `usize`
    const LEN: usize;
}

impl HasConstLen for () {
    const LEN: usize = 0;
}

impl<Head, Tail> HasConstLen for (Head, Tail)
where
    Tail: HasConstLen,
{
    const LEN: usize = 1 + Tail::LEN;
}

impl<Head, Tail> HasLen for (Head, Tail)
where
    Tail: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.1.len() + 1
    }
}

impl<Tail> HasLen for (Tail,)
where
    Tail: HasLen,
{
    #[inline]
    fn len(&self) -> usize {
        self.0.len()
    }
}

impl HasLen for () {
    #[inline]
    fn len(&self) -> usize {
        0
    }
}

/// Finds the `const_name` and `name_id`
pub trait HasNameId {
    /// Gets the `const_name` for this entry
    fn const_name(&self) -> &'static str;

    /// Gets the `name_id` for this entry
    fn name_id(&self) -> u64 {
        hash_std(self.const_name().as_bytes())
    }
}

/// Gets the id and `const_name` for the given index in a tuple
pub trait HasNameIdTuple: HasConstLen {
    /// Gets the `const_name` for the entry at the given index
    fn const_name_for(&self, index: usize) -> Option<&'static str>;

    /// Gets the `name_id` for the entry at the given index
    fn name_id_for(&self, index: usize) -> Option<u64>;
}

impl HasNameIdTuple for () {
    fn const_name_for(&self, _index: usize) -> Option<&'static str> {
        None
    }

    fn name_id_for(&self, _index: usize) -> Option<u64> {
        None
    }
}

impl<Head, Tail> HasNameIdTuple for (Head, Tail)
where
    Head: HasNameId,
    Tail: HasNameIdTuple,
{
    fn const_name_for(&self, index: usize) -> Option<&'static str> {
        if index == 0 {
            Some(self.0.const_name())
        } else {
            self.1.const_name_for(index - 1)
        }
    }

    fn name_id_for(&self, index: usize) -> Option<u64> {
        if index == 0 {
            Some(self.0.name_id())
        } else {
            self.1.name_id_for(index - 1)
        }
    }
}

/// Returns the first element with the given type
pub trait MatchFirstType {
    /// Returns the first element with the given type as borrow, or [`None`]
    fn match_first_type<T: 'static>(&self) -> Option<&T>;
    /// Returns the first element with the given type as mutable borrow, or [`None`]
    fn match_first_type_mut<T: 'static>(&mut self) -> Option<&mut T>;
}

impl MatchFirstType for () {
    fn match_first_type<T: 'static>(&self) -> Option<&T> {
        None
    }
    fn match_first_type_mut<T: 'static>(&mut self) -> Option<&mut T> {
        None
    }
}

impl<Head, Tail> MatchFirstType for (Head, Tail)
where
    Head: 'static,
    Tail: MatchFirstType,
{
    fn match_first_type<T: 'static>(&self) -> Option<&T> {
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            unsafe { (&raw const self.0 as *const T).as_ref() }
        } else {
            self.1.match_first_type::<T>()
        }
    }

    fn match_first_type_mut<T: 'static>(&mut self) -> Option<&mut T> {
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            unsafe { (&raw mut self.0 as *mut T).as_mut() }
        } else {
            self.1.match_first_type_mut::<T>()
        }
    }
}

/// Returns the first element with the given type (dereference mut version)
pub trait ExtractFirstRefType {
    /// Returns the first element with the given type as borrow, or [`None`]
    fn take<'a, T: 'static>(self) -> (Option<&'a T>, Self);
}

impl ExtractFirstRefType for () {
    fn take<'a, T: 'static>(self) -> (Option<&'a T>, Self) {
        tuple_list!(None)
    }
}

impl<Head, Tail> ExtractFirstRefType for (Option<&Head>, Tail)
where
    Head: 'static,
    Tail: ExtractFirstRefType,
{
    fn take<'a, T: 'static>(mut self) -> (Option<&'a T>, Self) {
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            let r = self.0.take();
            (unsafe { transmute::<Option<&Head>, Option<&T>>(r) }, self)
        } else {
            let (r, tail) = self.1.take::<T>();
            (r, (self.0, tail))
        }
    }
}

impl<Head, Tail> ExtractFirstRefType for (Option<&mut Head>, Tail)
where
    Head: 'static,
    Tail: ExtractFirstRefType,
{
    fn take<'a, T: 'static>(mut self) -> (Option<&'a T>, Self) {
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            let r = self.0.take();
            (
                unsafe { transmute::<Option<&mut Head>, Option<&T>>(r) },
                self,
            )
        } else {
            let (r, tail) = self.1.take::<T>();
            (r, (self.0, tail))
        }
    }
}

/// Returns the first element with the given type (dereference mut version)
pub trait ExtractFirstRefMutType {
    /// Returns the first element with the given type as borrow, or [`None`]
    fn take<'a, T: 'static>(self) -> (Option<&'a mut T>, Self);
}

impl ExtractFirstRefMutType for () {
    fn take<'a, T: 'static>(self) -> (Option<&'a mut T>, Self) {
        tuple_list!(None)
    }
}

impl<Head, Tail> ExtractFirstRefMutType for (Option<&mut Head>, Tail)
where
    Head: 'static,
    Tail: ExtractFirstRefMutType,
{
    fn take<'a, T: 'static>(mut self) -> (Option<&'a mut T>, Self) {
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            let r = self.0.take();
            (
                unsafe { transmute::<Option<&mut Head>, Option<&mut T>>(r) },
                self,
            )
        } else {
            let (r, tail) = self.1.take::<T>();
            (r, (self.0, tail))
        }
    }
}

/// Borrow each member of the tuple
pub trait SplitBorrowExtractFirstType<'a> {
    /// The Resulting [`TupleList`], of an [`SplitBorrow::borrow()`] call
    type SplitBorrowResult: ExtractFirstRefType;
    /// The Resulting [`TupleList`], of an [`SplitBorrow::borrow_mut()`] call
    type SplitBorrowMutResult: ExtractFirstRefType + ExtractFirstRefMutType;

    /// Return a tuple of borrowed references
    fn borrow(&'a self) -> Self::SplitBorrowResult;
    /// Return a tuple of borrowed mutable references
    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult;
}

impl<'a> SplitBorrowExtractFirstType<'a> for () {
    type SplitBorrowResult = ();
    type SplitBorrowMutResult = ();

    fn borrow(&'a self) -> Self::SplitBorrowResult {}

    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult {}
}

impl<'a, Head, Tail> SplitBorrowExtractFirstType<'a> for (Head, Tail)
where
    Head: 'static,
    Tail: SplitBorrowExtractFirstType<'a>,
{
    type SplitBorrowResult = (Option<&'a Head>, Tail::SplitBorrowResult);
    type SplitBorrowMutResult = (Option<&'a mut Head>, Tail::SplitBorrowMutResult);

    fn borrow(&'a self) -> Self::SplitBorrowResult {
        (Some(&self.0), self.1.borrow())
    }

    fn borrow_mut(&'a mut self) -> Self::SplitBorrowMutResult {
        (Some(&mut self.0), self.1.borrow_mut())
    }
}

/// Match by type
pub trait MatchType {
    /// Match by type and call the passed `f` function with a borrow, if found
    fn match_type<T: 'static, FN: FnMut(&T)>(&self, f: &mut FN);
    /// Match by type and call the passed `f` function with a mutable borrow, if found
    fn match_type_mut<T: 'static, FN: FnMut(&mut T)>(&mut self, f: &mut FN);
}

impl MatchType for () {
    /// Match by type and call the passed `f` function with a borrow, if found
    fn match_type<T: 'static, FN: FnMut(&T)>(&self, _: &mut FN) {}
    /// Match by type and call the passed `f` function with a mutable borrow, if found
    fn match_type_mut<T: 'static, FN: FnMut(&mut T)>(&mut self, _: &mut FN) {}
}

impl<Head, Tail> MatchType for (Head, Tail)
where
    Head: 'static,
    Tail: MatchType,
{
    fn match_type<T: 'static, FN: FnMut(&T)>(&self, f: &mut FN) {
        // Switch this check to https://stackoverflow.com/a/60138532/7658998 when in stable and remove 'static
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            f(unsafe { (&raw const self.0 as *const T).as_ref() }.unwrap());
        }
        self.1.match_type::<T, FN>(f);
    }

    fn match_type_mut<T: 'static, FN: FnMut(&mut T)>(&mut self, f: &mut FN) {
        // Switch this check to https://stackoverflow.com/a/60138532/7658998 when in stable and remove 'static
        if TypeId::of::<T>() == TypeId::of::<Head>() {
            f(unsafe { (&raw mut self.0 as *mut T).as_mut() }.unwrap());
        }
        self.1.match_type_mut::<T, FN>(f);
    }
}

#[cfg(feature = "alloc")]
/// A named tuple
pub trait NamedTuple {
    /// Gets the name of this tuple
    fn name(&self, index: usize) -> Option<&Cow<'static, str>>;

    /// Gets all the names
    fn names(&self) -> Vec<Cow<'static, str>>;
}

#[cfg(feature = "alloc")]
impl NamedTuple for () {
    fn name(&self, _index: usize) -> Option<&Cow<'static, str>> {
        None
    }

    fn names(&self) -> Vec<Cow<'static, str>> {
        Vec::new()
    }
}

#[cfg(feature = "alloc")]
impl Named for () {
    #[inline]
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("Empty");
        &NAME
    }
}

#[cfg(feature = "alloc")]
impl<Head, Tail> NamedTuple for (Head, Tail)
where
    Head: Named,
    Tail: NamedTuple,
{
    fn name(&self, index: usize) -> Option<&Cow<'static, str>> {
        if index == 0 {
            Some(self.0.name())
        } else {
            self.1.name(index - 1)
        }
    }

    fn names(&self) -> Vec<Cow<'static, str>> {
        let first = self.0.name().clone();
        let mut last = self.1.names();
        last.insert(0, first);
        last
    }
}

/// Match for a name and return the value
#[cfg(feature = "alloc")]
pub trait MatchName {
    /// Match for a name and return the borrowed value
    #[deprecated = "Use `.handle` and either `.get` (fallible access) or `[]` (infallible access) instead"]
    fn match_name<T>(&self, name: &str) -> Option<&T>;
    /// Match for a name and return the mut borrowed value
    #[deprecated = "Use `.handle` and either `.get` (fallible access) or `[]` (infallible access) instead"]
    fn match_name_mut<T>(&mut self, name: &str) -> Option<&mut T>;
}

#[cfg(feature = "alloc")]
impl MatchName for () {
    fn match_name<T>(&self, _name: &str) -> Option<&T> {
        None
    }
    fn match_name_mut<T>(&mut self, _name: &str) -> Option<&mut T> {
        None
    }
}

#[cfg(feature = "alloc")]
#[expect(deprecated)]
impl<Head, Tail> MatchName for (Head, Tail)
where
    Head: Named,
    Tail: MatchName,
{
    fn match_name<T>(&self, name: &str) -> Option<&T> {
        if type_eq::<Head, T>() && name == self.0.name() {
            unsafe { (&raw const self.0 as *const T).as_ref() }
        } else {
            self.1.match_name::<T>(name)
        }
    }

    fn match_name_mut<T>(&mut self, name: &str) -> Option<&mut T> {
        if type_eq::<Head, T>() && name == self.0.name() {
            unsafe { (&raw mut self.0 as *mut T).as_mut() }
        } else {
            self.1.match_name_mut::<T>(name)
        }
    }
}

/// Structs that have a [`Handle`] to reference this element by, in maps.
/// You should use this when you want to avoid specifying types.
#[cfg(feature = "alloc")]
pub trait Handled: Named {
    /// Return the [`Handle`]
    fn handle(&self) -> Handle<Self> {
        Handle {
            name: Named::name(self).clone(),
            phantom: PhantomData,
        }
    }
}

#[cfg(feature = "alloc")]
impl<N> Handled for N where N: Named {}

/// Object with the type T and the name associated with its concrete value
#[derive(Serialize, Deserialize)]
#[cfg(feature = "alloc")]
pub struct Handle<T: ?Sized> {
    name: Cow<'static, str>,
    #[serde(skip)]
    phantom: PhantomData<T>,
}

#[cfg(feature = "alloc")]
impl<T: ?Sized> Handle<T> {
    /// Create a new [`Handle`] with the given name.
    #[must_use]
    pub fn new(name: Cow<'static, str>) -> Self {
        Self {
            name,
            phantom: PhantomData,
        }
    }

    /// Fetch the name of the referenced instance.
    ///
    /// We explicitly do *not* implement [`Named`], as this could potentially lead to confusion
    /// where we make a [`Handle`] of a [`Handle`] as [`Named`] is blanket implemented.
    #[must_use]
    pub fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

#[cfg(feature = "alloc")]
impl<T> Clone for Handle<T> {
    fn clone(&self) -> Self {
        Self {
            name: self.name.clone(),
            phantom: PhantomData,
        }
    }
}

#[cfg(feature = "alloc")]
impl<T> Debug for Handle<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("Handle")
            .field("name", self.name())
            .field("type", &type_name::<T>())
            .finish()
    }
}

/// Search using `Handle `
#[cfg(feature = "alloc")]
pub trait MatchNameRef {
    /// Search using name and `Handle `
    fn get<T>(&self, rf: &Handle<T>) -> Option<&T>;

    /// Search using name and `Handle `
    fn get_mut<T>(&mut self, rf: &Handle<T>) -> Option<&mut T>;
}

/// Get multiple values using a tuple of handles
///
/// # Example
/// ```ignore
/// let tuple = tuple_list!(a, b, c);
/// let handles = tuple_list!(&a_handle, &c_handle);
/// let tuple_list!(a_ref, c_ref) = tuple.get_all(handles);
/// ```
#[cfg(feature = "alloc")]
pub trait GetAll<HandleTuple> {
    /// The result type of getting all values
    type GetAllResult<'a>
    where
        Self: 'a;

    /// The result type of getting all mutable values
    type GetAllMutResult<'a>
    where
        Self: 'a;

    /// Get all values from a tuple of handles, the order is preserved
    fn get_all(&self, handles: HandleTuple) -> Self::GetAllResult<'_>;

    /// Get all mutable values from a tuple of handles, the order is preserved
    fn get_all_mut(&mut self, handles: HandleTuple) -> Self::GetAllMutResult<'_>;
}

#[cfg(feature = "alloc")]
#[expect(deprecated)]
impl<M> MatchNameRef for M
where
    M: MatchName,
{
    fn get<T>(&self, rf: &Handle<T>) -> Option<&T> {
        self.match_name::<T>(&rf.name)
    }

    fn get_mut<T>(&mut self, rf: &Handle<T>) -> Option<&mut T> {
        self.match_name_mut::<T>(&rf.name)
    }
}

#[cfg(feature = "alloc")]
impl<Head, Tail> GetAll<()> for (Head, Tail)
where
    (Head, Tail): MatchNameRef,
{
    type GetAllResult<'a>
        = ()
    where
        Self: 'a;

    type GetAllMutResult<'a>
        = ()
    where
        Self: 'a;

    fn get_all(&self, _handles: ()) -> Self::GetAllResult<'_> {}

    fn get_all_mut(&mut self, _handles: ()) -> Self::GetAllMutResult<'_> {}
}

#[cfg(feature = "alloc")]
mod seal {
    //! The logic in this section enables the [`super::GetAll::get_all_mut`] implementation.
    //!
    //! This works by treating tuple lists like queues; first, we convert a mutable ref to a tuple
    //! list into a tuple list of mutable refs. With this, we then check each member of the tuple
    //! list against the head of the handle list. If the head doesn't match, we append the head to
    //! a list of already visited nodes. If the head matches, we extract the head and restart
    //! traversal _without_ the head by merging the visited list with the tail of the current list.
    //! If we ever hit the tail of the current list, we pop the top handle off and continue the
    //! search from the start.
    //!
    //! Visualised, you can think of this something like this:
    //!
    //! ```text
    //! handles:    [a, c, e]
    //! tuple list: [a, b, c, d]
    //! visited:    []
    //!
    //! 'a' matches 'a', so we stash 'a' continue with:
    //! handles:    [c, e]
    //! tuple list: [b, c, d]
    //! visited:    []
    //!
    //! 'c' does not match 'b', so we push to the visited list and continue with:
    //! handles:    [c, e]
    //! tuple list: [c, d]
    //! visited:    [b]
    //!
    //! 'c' matches; stash 'c', merge visited to list, and continue:
    //! handles:    [e]
    //! tuple list: [b, d]
    //! visited:    []
    //!
    //! 'b' and 'd' do not match, so we make it to the base case:
    //! handles:    [e]
    //! tuple list: []
    //! visited:    [b, d]
    //!
    //! We set the 'e' match to None, merge visited to list, then continue:
    //! handles:    []
    //! tuple list: [b, d]
    //! visited:    []
    //!
    //! Handles is now empty, so we return () (list terminator) and end up with:
    //! [Some(a), Some(c), None]
    //! ```

    use tuple_list::tuple_list;

    use crate::{
        Named,
        tuples::{Handle, Merge, type_eq},
    };

    pub trait InnerBorrowMut {
        type Borrowed<'a>
        where
            Self: 'a;

        fn inner_borrow_mut(&mut self) -> Self::Borrowed<'_>;
    }

    impl<Head, Tail> InnerBorrowMut for (Head, Tail)
    where
        Tail: InnerBorrowMut,
    {
        type Borrowed<'a>
            = (&'a mut Head, Tail::Borrowed<'a>)
        where
            Self: 'a;

        fn inner_borrow_mut(&mut self) -> Self::Borrowed<'_> {
            (&mut self.0, self.1.inner_borrow_mut())
        }
    }

    impl InnerBorrowMut for () {
        type Borrowed<'a>
            = ()
        where
            Self: 'a;

        fn inner_borrow_mut(&mut self) -> Self::Borrowed<'_> {}
    }

    pub trait StackedExtract<'a, Handles, Visited> {
        type Result;

        fn extract(self, handles: Handles, visited: Visited) -> Self::Result;
    }

    impl<'a, T, HTail, Visited> StackedExtract<'a, (&Handle<T>, HTail), Visited> for ()
    where
        Visited: StackedExtract<'a, HTail, ()>,
        T: 'a,
    {
        type Result = (Option<&'a mut T>, Visited::Result);

        fn extract(self, handles: (&Handle<T>, HTail), visited: Visited) -> Self::Result {
            (None, visited.extract(handles.1, ()))
        }
    }

    impl<U, Visited> StackedExtract<'_, (), Visited> for U {
        type Result = ();

        fn extract(self, _handles: (), _visited: Visited) -> Self::Result {}
    }

    impl<'a, T, HTail, Head, Tail, Visited> StackedExtract<'a, (&Handle<T>, HTail), Visited>
        for (&'a mut Head, Tail)
    where
        Head: Named,
        Tail: for<'b> StackedExtract<
                'a,
                (&'b Handle<T>, HTail),
                <Visited as Merge<(&'a mut Head, ())>>::MergeResult,
                Result = (
                    Option<&'a mut T>,
                    <<Visited as Merge<Tail>>::MergeResult as StackedExtract<'a, HTail, ()>>::Result,
                ),
            >,
        Visited: Merge<(&'a mut Head, ())>,
        Visited: Merge<Tail>,
        <Visited as Merge<Tail>>::MergeResult: StackedExtract<'a, HTail, ()>,
        T: 'a,
    {
        type Result = (
            Option<&'a mut T>,
            <<Visited as Merge<Tail>>::MergeResult as StackedExtract<'a, HTail, ()>>::Result,
        );

        fn extract(self, handles: (&Handle<T>, HTail), visited: Visited) -> Self::Result {
            if type_eq::<Head, T>() && &handles.0.name == self.0.name() {
                (
                    unsafe { (core::ptr::from_mut(self.0) as *mut T).as_mut() },
                    visited.merge(self.1).extract(handles.1, ()),
                )
            } else {
                self.1.extract(handles, visited.merge(tuple_list!(self.0)))
            }
        }
    }
}

#[cfg(feature = "alloc")]
impl<'b, Head, Tail, T, HandleTail> GetAll<(&'b Handle<T>, HandleTail)> for (Head, Tail)
where
    (Head, Tail): MatchNameRef + GetAll<HandleTail> + InnerBorrowMut,
    for<'a> <(Head, Tail) as InnerBorrowMut>::Borrowed<'a>:
        StackedExtract<'a, (&'b Handle<T>, HandleTail), ()>,
    T: 'static,
{
    type GetAllResult<'a>
        = (
        Option<&'a T>,
        <(Head, Tail) as GetAll<HandleTail>>::GetAllResult<'a>,
    )
    where
        Self: 'a;

    type GetAllMutResult<'a>
        = <<(Head, Tail) as InnerBorrowMut>::Borrowed<'a> as StackedExtract<
        'a,
        (&'b Handle<T>, HandleTail),
        (),
    >>::Result
    where
        Self: 'a;

    fn get_all(&self, handles: (&Handle<T>, HandleTail)) -> Self::GetAllResult<'_> {
        let (head_handle, tail_handles) = handles;
        let head_result = self.get(head_handle);
        let tail_result = <(Head, Tail) as GetAll<HandleTail>>::get_all(self, tail_handles);
        (head_result, tail_result)
    }

    fn get_all_mut(&mut self, handles: (&'b Handle<T>, HandleTail)) -> Self::GetAllMutResult<'_> {
        self.inner_borrow_mut().extract(handles, ())
    }
}

/// A wrapper type to enable the indexing of [`MatchName`] implementors with `[]`.
#[cfg(feature = "alloc")]
#[derive(Debug, Copy, Clone)]
#[repr(transparent)]
pub struct RefIndexable<RM, M>(RM, PhantomData<M>);

#[cfg(feature = "alloc")]
impl<RM, M> From<RM> for RefIndexable<RM, M>
where
    RM: Deref<Target = M>,
{
    fn from(value: RM) -> Self {
        RefIndexable(value, PhantomData)
    }
}

#[cfg(feature = "alloc")]
impl<RM, M> Deref for RefIndexable<RM, M>
where
    RM: Deref<Target = M>,
{
    type Target = RM::Target;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

#[cfg(feature = "alloc")]
impl<RM, M> DerefMut for RefIndexable<RM, M>
where
    RM: DerefMut<Target = M>,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

#[cfg(feature = "alloc")]
impl<T, RM, M> Index<&Handle<T>> for RefIndexable<RM, M>
where
    RM: Deref<Target = M>,
    M: MatchName,
{
    type Output = T;

    fn index(&self, index: &Handle<T>) -> &Self::Output {
        let Some(e) = self.get(index) else {
            panic!("Could not find entry matching {index:?}")
        };
        e
    }
}

#[cfg(feature = "alloc")]
impl<T, RM, M> IndexMut<&Handle<T>> for RefIndexable<RM, M>
where
    RM: DerefMut<Target = M>,
    M: MatchName,
{
    fn index_mut(&mut self, index: &Handle<T>) -> &mut Self::Output {
        let Some(e) = self.get_mut(index) else {
            panic!("Could not find entry matching {index:?}")
        };
        e
    }
}

/// Allows prepending of values to a tuple
pub trait Prepend<T> {
    /// Prepend a value to this tuple, returning a new tuple with prepended value.
    #[must_use]
    fn prepend(self, value: T) -> (T, Self);
}

/// Implement prepend for tuple list.
impl<Tail, T> Prepend<T> for Tail {
    fn prepend(self, value: T) -> (T, Self) {
        (value, self)
    }
}

/// Append to a tuple
pub trait Append<T>
where
    Self: Sized,
{
    /// Append Value and return the tuple
    #[must_use]
    fn append(self, value: T) -> (Self, T);
}

/// Implement append for tuple list.
impl<Head, T> Append<T> for Head {
    fn append(self, value: T) -> (Self, T) {
        (self, value)
    }
}

/// Merge two `TupleList`
pub trait Merge<T> {
    /// The Resulting [`TupleList`], of an [`Merge::merge()`] call
    type MergeResult;

    /// Merge and return the merged tuple
    #[must_use]
    fn merge(self, value: T) -> Self::MergeResult;
}

/// Implement merge for an empty tuple list.
impl<T> Merge<T> for () {
    type MergeResult = T;

    fn merge(self, value: T) -> Self::MergeResult {
        value
    }
}

/// Implement merge for non-empty tuple list.
impl<Head, Tail, T> Merge<T> for (Head, Tail)
where
    Tail: Merge<T>,
{
    type MergeResult = (Head, Tail::MergeResult);

    fn merge(self, value: T) -> Self::MergeResult {
        let (head, tail) = self;
        (head, tail.merge(value))
    }
}

/// Trait for structs which are capable of mapping a given type to another.
pub trait MappingFunctor<T> {
    /// The result of the mapping operation.
    type Output;

    /// The actual mapping operation.
    fn apply(&mut self, from: T) -> Self::Output;
}

/// Map all entries in a tuple to another type, dependent on the tail type.
pub trait Map<M> {
    /// The result of the mapping operation.
    type MapResult;

    /// Perform the mapping!
    fn map(self, mapper: M) -> Self::MapResult;
}

impl<Head, Tail, M> Map<M> for (Head, Tail)
where
    M: MappingFunctor<Head>,
    Tail: Map<M>,
{
    type MapResult = (M::Output, Tail::MapResult);

    fn map(self, mut mapper: M) -> Self::MapResult {
        let head = mapper.apply(self.0);
        (head, self.1.map(mapper))
    }
}

impl<M> Map<M> for () {
    type MapResult = ();

    fn map(self, _mapper: M) -> Self::MapResult {}
}

/// Iterate over a tuple, executing the given `expr` for each element.
#[macro_export]
macro_rules! tuple_for_each {
    ($fn_name:ident, $trait_name:path, $tuple_name:ident, $body:expr) => {
        mod $fn_name {
            pub trait ForEach {
                fn for_each(&self);
            }

            impl ForEach for () {
                fn for_each(&self) {}
            }

            impl<Head, Tail> ForEach for (Head, Tail)
            where
                Head: $trait_name,
                Tail: tuple_list::TupleList + ForEach,
            {
                #[allow(clippy::redundant_closure_call)] // macro may be called on a closure or a function
                fn for_each(&self) {
                    ($body)(&self.0);
                    self.1.for_each();
                }
            }
        }
        {
            use $fn_name::*;

            $tuple_name.for_each();
        };
    };
}

/// Iterate over a tuple, executing the given `expr` for each element, granting mut access.
#[macro_export]
macro_rules! tuple_for_each_mut {
    ($fn_name:ident, $trait_name:path, $tuple_name:ident, $body:expr) => {
        mod $fn_name {
            pub trait ForEachMut {
                fn for_each_mut(&mut self);
            }

            impl ForEachMut for () {
                fn for_each_mut(&mut self) {}
            }

            impl<Head, Tail> ForEachMut for (Head, Tail)
            where
                Head: $trait_name,
                Tail: tuple_list::TupleList + ForEachMut,
            {
                #[allow(clippy::redundant_closure_call)] // macro may be called on a closure or a function
                fn for_each_mut(&mut self) {
                    ($body)(&mut self.0);
                    self.1.for_each_mut();
                }
            }
        }
        {
            use $fn_name::*;

            $tuple_name.for_each_mut();
        };
    };
}

/// Maps the types of a mapping with a [`MappingFunctor`]
///
/// ```rust
/// use libafl_bolts::{
///     map_tuple_list_type,
///     tuples::{Map, MappingFunctor, tuple_list, tuple_list_type},
/// };
///
/// struct Wrapper<T>(T);
/// struct MyMapper;
///
/// impl<T> MappingFunctor<T> for MyMapper {
///     type Output = Wrapper<T>;
///
///     fn apply(&mut self, from: T) -> <Self as MappingFunctor<T>>::Output {
///         Wrapper(from)
///     }
/// }
///
/// struct A;
/// struct B;
/// struct C;
///
/// type OrigType = tuple_list_type!(A, B, C);
/// type MappedType = map_tuple_list_type!(OrigType, MyMapper);
/// let orig: OrigType = tuple_list!(A, B, C);
/// let _mapped: MappedType = orig.map(MyMapper);
/// ```
#[macro_export]
macro_rules! map_tuple_list_type {
    ($Tuple:ty, $Mapper:ty) => {
        <$Tuple as $crate::tuples::Map<$Mapper>>::MapResult
    };
}

/// Merges the types of two merged [`tuple_list!`]s
///
/// ```rust
/// use libafl_bolts::{
///     merge_tuple_list_type,
///     tuples::{Merge, tuple_list, tuple_list_type},
/// };
///
/// struct A;
/// struct B;
/// struct C;
/// struct D;
/// struct E;
///
/// type Lhs = tuple_list_type!(A, B, C);
/// type Rhs = tuple_list_type!(D, E);
/// type Merged = merge_tuple_list_type!(Lhs, Rhs);
///
/// let lhs: Lhs = tuple_list!(A, B, C);
/// let rhs: Rhs = tuple_list!(D, E);
/// let _merged: Merged = lhs.merge(rhs);
/// ```
#[macro_export]
macro_rules! merge_tuple_list_type {
   // Base case: when only two types are provided, apply the Merge trait directly
   ($Type1:ty) => {
        $Type1
    };

   // Base case: when only two types are provided, apply the Merge trait directly
   ($Type1:ty, $Type2:ty) => {
        <$Type1 as $crate::tuples::Merge<$Type2>>::MergeResult
    };

    // Recursive case: when more than two types are provided
    ($Type1:ty, $Type2:ty, $( $rest:ty ),+) => {
        merge_tuple_list_type!(
            <$Type1 as $crate::tuples::Merge<$Type2>>::MergeResult,
            $( $rest ),+
        )
    };
}

/*

// Define trait and implement it for several primitive types.
trait PlusOne {
    fn plus_one(&mut self);
}
impl PlusOne for i32    { fn plus_one(&mut self) { *self += 1; } }
impl PlusOne for String { fn plus_one(&mut self) { self.push('1'); } }

// Now we have to implement trait for an empty tuple,
// thus defining initial condition.
impl PlusOne for () {
    fn plus_one(&mut self) {}
}

// Now we can implement trait for a non-empty tuple list,
// thus defining recursion and supporting tuple lists of arbitrary length.
impl<Head, Tail> PlusOne for (Head, Tail) where
    Head: PlusOne,
    Tail: PlusOne + TupleList,
{
    fn plus_one(&mut self) {
        self.0.plus_one();
        self.1.plus_one();
    }
}

*/

#[cfg(test)]
mod test {
    use core::marker::PhantomData;

    use tuple_list::{tuple_list, tuple_list_type};

    #[cfg(feature = "alloc")]
    use crate::ownedref::OwnedMutSlice;
    use crate::tuples::{Map, MappingFunctor, Merge, type_eq};

    #[test]
    // for type name tests
    fn test_type_eq_simple() {
        // test eq
        assert!(type_eq::<u64, u64>());

        // test neq
        assert!(!type_eq::<u64, usize>());
    }

    #[test]
    fn test_type_eq() {
        // An alias for equality testing
        type OwnedMutSliceAlias<'a> = OwnedMutSlice<'a, u8>;

        // A function for lifetime testing
        #[expect(clippy::extra_unused_lifetimes)]
        fn test_lifetimes<'a, 'b>() {
            assert!(type_eq::<OwnedMutSlice<'a, u8>, OwnedMutSlice<'b, u8>>());
            assert!(type_eq::<OwnedMutSlice<'static, u8>, OwnedMutSlice<'a, u8>>());
            assert!(type_eq::<OwnedMutSlice<'a, u8>, OwnedMutSlice<'b, u8>>());
            assert!(type_eq::<OwnedMutSlice<'a, u8>, OwnedMutSlice<'static, u8>>());
            assert!(!type_eq::<OwnedMutSlice<'a, u8>, OwnedMutSlice<'b, i8>>());
        }
        assert!(type_eq::<OwnedMutSlice<u8>, OwnedMutSliceAlias>());

        test_lifetimes();
        // test weirder lifetime things
        assert!(type_eq::<OwnedMutSlice<u8>, OwnedMutSlice<u8>>());
        assert!(!type_eq::<OwnedMutSlice<u8>, OwnedMutSlice<u32>>());
    }

    #[test]
    fn test_mapper() {
        struct W<T>(T);

        // PhantomData shows how to deal with mappers that have generics
        struct ExampleMapper<P>(PhantomData<P>);

        impl<T, P> MappingFunctor<T> for ExampleMapper<P> {
            type Output = W<T>;

            fn apply(&mut self, from: T) -> Self::Output {
                W(from)
            }
        }

        struct A;
        struct B;
        struct C;

        type OrigType = tuple_list_type!(A, B, C);
        type MappedType = map_tuple_list_type!(OrigType, ExampleMapper<usize>);
        let orig: OrigType = tuple_list!(A, B, C);
        let _mapped: MappedType = orig.map(ExampleMapper(PhantomData::<usize>));
    }

    #[test]
    fn test_merge() {
        struct A;
        struct B;
        struct C;
        struct D;
        struct E;

        type Lhs = tuple_list_type!(A, B, C);
        type Rhs = tuple_list_type!(D, E);
        type Merged = merge_tuple_list_type!(Lhs, Rhs);
        type IndividuallyMergedPre = merge_tuple_list_type!(
            tuple_list_type!(A),
            tuple_list_type!(B),
            tuple_list_type!(C),
            Rhs
        );
        type IndividuallyMergedPost =
            merge_tuple_list_type!(Lhs, tuple_list_type!(D), tuple_list_type!(E));
        type MergedCloned = merge_tuple_list_type!(Merged);

        let lhs: Lhs = tuple_list!(A, B, C);
        let rhs: Rhs = tuple_list!(D, E);
        let merged: Merged = lhs.merge(rhs);
        let merged: IndividuallyMergedPre = merged;
        let merged: IndividuallyMergedPost = merged;
        #[allow(clippy::no_effect_underscore_binding)]
        let _merged: MergedCloned = merged;
    }

    /// Function that tests the tuple macros
    #[test]
    #[cfg(feature = "std")]
    fn test_macros() {
        let mut t = tuple_list!(1, "a");

        tuple_for_each!(f1, core::fmt::Display, t, |x| {
            log::info!("{x}");
        });

        tuple_for_each_mut!(f2, core::fmt::Display, t, |x| {
            log::info!("{x}");
        });
    }

    #[cfg(feature = "alloc")]
    mod get_all {
        use alloc::{
            borrow::Cow,
            string::{String, ToString as _},
        };

        use tuple_list::{tuple_list, tuple_list_type};

        use crate::tuples::{GetAll as _, Handle, Handled as _, MatchNameRef, Named};

        #[derive(Debug, PartialEq)]
        struct MyHandled(String, Cow<'static, str>);
        impl Named for MyHandled {
            fn name(&self) -> &Cow<'static, str> {
                &self.1
            }
        }

        impl MyHandled {
            fn new(name: &str) -> Self {
                Self(name.to_string(), Cow::Owned(name.to_string()))
            }
        }

        #[expect(clippy::type_complexity)]
        fn get_tuple() -> (
            tuple_list_type!(MyHandled, MyHandled, MyHandled),
            (Handle<MyHandled>, Handle<MyHandled>, Handle<MyHandled>),
        ) {
            let a = MyHandled::new("a");
            let a_handle = a.handle();
            let b = MyHandled::new("b");
            let b_handle = b.handle();
            let c = MyHandled::new("c");
            let c_handle = c.handle();
            (tuple_list!(a, b, c), (a_handle, b_handle, c_handle))
        }

        #[test]
        fn test_get_all_empty() {
            let (tuple, _handles) = get_tuple();
            #[expect(clippy::let_unit_value)]
            let recovered = tuple.get_all(tuple_list!());
            #[allow(clippy::unit_cmp)]
            // needs its own scope to make the clippy expect work
            {
                assert_eq!(recovered, ());
            }
        }

        #[test]
        fn test_get_all_one() {
            let (tuple, handles) = get_tuple();
            let tuple_list!(a_rec) = tuple.get_all(tuple_list!(&handles.0));
            assert_eq!(a_rec, Some(&tuple.0));
        }

        #[test]
        fn test_get_all_two() {
            let (tuple, handles) = get_tuple();
            let tuple_list!(a_rec, b_rec) = tuple.get_all(tuple_list!(&handles.0, &handles.1));
            assert_eq!(a_rec, Some(&tuple.0));
            assert_eq!(b_rec, Some(&tuple.1.0));
        }

        #[test]
        fn test_get_all_all() {
            let (tuple, handles) = get_tuple();
            let tuple_list!(a_rec, b_rec, c_rec) =
                tuple.get_all(tuple_list!(&handles.0, &handles.1, &handles.2));
            assert_eq!(a_rec, Some(&tuple.0));
            assert_eq!(b_rec, Some(&tuple.1.0));
            assert_eq!(c_rec, Some(&tuple.1.1.0));
        }

        #[test]
        fn test_get_all_reverse() {
            let (tuple, handles) = get_tuple();
            let tuple_list!(c_rec, b_rec, a_rec) =
                tuple.get_all(tuple_list!(&handles.2, &handles.1, &handles.0));
            assert_eq!(c_rec, Some(&tuple.1.1.0));
            assert_eq!(b_rec, Some(&tuple.1.0));
            assert_eq!(a_rec, Some(&tuple.0));
        }

        #[test]
        fn test_get_all_mut_empty() {
            let mut tuple = get_tuple().0;
            #[expect(clippy::let_unit_value)]
            let recovered = tuple.get_all_mut(tuple_list!());
            #[allow(clippy::unit_cmp)]
            // needs its own scope to make the clippy expect work
            {
                assert_eq!(recovered, tuple_list!());
            }
        }

        #[test]
        fn test_get_all_mut_one() {
            let (mut tuple, handles) = get_tuple();
            let recovered = tuple.get_all_mut(tuple_list!(&handles.0));
            let tuple_list!(a_rec) = recovered;
            a_rec.unwrap().0.push_str("_modified_a");
            assert_eq!(tuple.get(&handles.0).unwrap().0, "a_modified_a");
        }

        #[test]
        fn test_get_all_mut_two() {
            let (mut tuple, handles) = get_tuple();
            let tuple_list!(a_rec, b_rec) = tuple.get_all_mut(tuple_list!(&handles.0, &handles.1));
            a_rec.unwrap().0.push_str("_modified_a");
            b_rec.unwrap().0.push_str("_modified_b");
            assert_eq!(tuple.get(&handles.0).unwrap().0, "a_modified_a");
            assert_eq!(tuple.get(&handles.1).unwrap().0, "b_modified_b");
        }

        #[test]
        fn test_get_all_mut_all() {
            let (mut tuple, handles) = get_tuple();
            let tuple_list!(a_rec, b_rec, c_rec) =
                tuple.get_all_mut(tuple_list!(&handles.0, &handles.1, &handles.2));
            a_rec.unwrap().0.push_str("_modified_a");
            b_rec.unwrap().0.push_str("_modified_b");
            c_rec.unwrap().0.push_str("_modified_c");
            assert_eq!(tuple.get(&handles.0).unwrap().0, "a_modified_a");
            assert_eq!(tuple.get(&handles.1).unwrap().0, "b_modified_b");
            assert_eq!(tuple.get(&handles.2).unwrap().0, "c_modified_c");
        }

        #[test]
        fn test_get_all_mut_reverse() {
            let (mut tuple, handles) = get_tuple();
            let tuple_list!(c_rec, b_rec, a_rec) =
                tuple.get_all_mut(tuple_list!(&handles.2, &handles.1, &handles.0));
            c_rec.unwrap().0.push_str("_modified_c");
            b_rec.unwrap().0.push_str("_modified_b");
            a_rec.unwrap().0.push_str("_modified_a");
            assert_eq!(tuple.get(&handles.0).unwrap().0, "a_modified_a");
            assert_eq!(tuple.get(&handles.1).unwrap().0, "b_modified_b");
            assert_eq!(tuple.get(&handles.2).unwrap().0, "c_modified_c");
        }

        #[test]
        fn test_get_all_mut_unsafe() {
            let (mut tuple, handles) = get_tuple();
            let handle_1 = handles.0;
            let handle_1_clone = handle_1.clone();

            // pass the same handle twice, get two mutable references to the same value
            // cannot pattern match to tuple_list, because it cannot deal with mut
            let tuple_list!(rec_1, rec_1_clone) =
                tuple.get_all_mut(tuple_list!(&handle_1, &handle_1_clone));
            assert!(rec_1.is_some());
            assert!(rec_1_clone.is_none());
        }

        #[test]
        fn test_preserve_order() {
            let a = MyHandled::new("a");
            let a_handle = a.handle();
            let b = MyHandled::new("a"); // note: same name
            let b_handle = b.handle();

            let mut tuple = tuple_list!(a, b);

            // pass the same handle twice, get two mutable references to the same value
            // cannot pattern match to tuple_list, because it cannot deal with mut
            let tuple_list!(_rec_a, rec_b) = tuple.get_all_mut(tuple_list!(&a_handle, &b_handle));

            rec_b.unwrap().0 = "b".to_string();

            let tuple_list!(a, b) = tuple;
            assert_eq!(a.0, "a");
            assert_eq!(b.0, "b");
        }
    }
}
