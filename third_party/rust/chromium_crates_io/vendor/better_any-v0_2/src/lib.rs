#![warn(missing_docs)]
#![warn(rust_2018_idioms)]
#![cfg_attr(feature = "nightly", feature(coerce_unsized))]
#![cfg_attr(feature = "nightly", feature(ptr_metadata))]
//! # Better Any
//!
//! Rust RFC for `non_static_type_id` feature has been reverted.
//! Which means in foreseeable future there will be no built-in way in rust to get type id for non-static type
//! let alone safely use it to downcast to a particular type.
//!
//! This crate provides tools to do these things safely for types with single lifetime.
//! Although looks like it is technically possible to extend this approach for multiple lifetimes,
//! consistent api and derive macro would be much harder to create and use because of the necessity
//! to properly handle lifetime relations.
//! Feel free to create an issue if you have actual use case where you need this functionality for multiple lifetimes.
//!
//! Also it has better downcasting that allows you do downcast not just from `dyn Tid` (like `dyn Any`) but from
//! any trait object that implements [`Tid`].
//! So there is no more need to extend your traits with` fn to_any(&self)-> &dyn Any`
//!
//! MSRV: `1.41.0-stable` (without nightly feature)
//!
//! ### Usage
//!
//! Basically in places where before you have used `dyn Any` you can use `dyn Tid<'a>`
//!  - If your type is generic you should derive `Tid` implementation for it with `tid!` macro or `Tid` derive macro.
//! Then to retrieve back concrete type `<dyn Tid>::downcast_*` methods should be used.
//!  - If your type is not generic/implements Any you can create `dyn Tid` from it via any of the available `From` implementations.
//! Then to retrieve back concrete type `<dyn Tid>::downcast_any_*` methods should be used
//!  - If your type is not generic and local to your crate you also can derive `Tid` but then you need to be careful
//! to use methods that corresponds to the way you create `dyn Tid` for that particular type.
//! Otherwise downcasting will return `None`.
//!
//! If all your types can implement `Tid` to avoid confusion
//! recommended way is to use first option even if some types implement `Any`.
//! If there are some types that implement `Any` and can't implement `Tid` (i.e. types from other library),
//! recommended way is to use second option for all types that implement `Any` to reduce confusion to minimum.
//!
//! ### Interoperability with Any
//!
//! Unfortunately you can't just use `Tid` everywhere because currently it is impossible
//! to implement `Tid` for `T:Any` since it would conflict with any other possible `Tid` implementation.
//! To overcome this limitation there is a `From` impl to go from `Box/&/&mut T where T:Any` to `Box/&/&mut dyn Tid`.
//!
//! Nevertheless if you are using `dyn Trait` where `Trait:Tid` all of this wouldn't work,
//! and you are left with `Tid` only.
//!
//! ### Safety
//!
//! It is safe because created trait object preserves lifetime information,
//! thus allowing us to safely downcast with proper lifetime.
//! Otherwise internally it is plain old `Any`.
use std::any::{Any, TypeId};

/// Attribute macro that makes your implementation of `TidAble` safe
/// Use it when you can't use derive e.g. for trait object.
///
/// ```rust
/// # use better_any::{TidAble,impl_tid};
/// trait Trait<'a>{}
/// #[impl_tid]
/// impl<'a> TidAble<'a> for Box<dyn Trait<'a> + 'a>{}
/// ```
#[deprecated(since = "0.2", note = "use tid! macro instead")]
#[cfg(feature = "derive")]
pub use better_typeid_derive::impl_tid;

/// Derive macro to implement traits from this crate
///
/// It checks if it is safe to implement `Tid` for your struct
/// Also it adds `:TidAble<'a>` bound on type parameters
/// unless your type parameter already has **explicit** `'static` bound
///
/// All of its functionality is available via regular `tid!` macro,
/// so unless you really want looks/readability of derive macro,
/// there is no need to drag whole proc-macro machinery to your project.
#[cfg(feature = "derive")]
pub use better_typeid_derive::Tid;

/// This trait indicates that you can substitute this type as a type parameter to
/// another type so that resulting type could implement `Tid`.
///
/// So if you don't have such generic types, just use `Tid` everywhere,
/// you don't need to use this trait at all.
///
/// Only this trait is actually being implemented on user side.
/// Other traits are mostly just blanket implementations over X:TidAble<'a>
///
/// Note that this trait interferes with object safety, so you shouldn't use it as a super trait
/// if you are going to make a trait object. Formally it is still object safe,
/// but you can't make a trait object from it without specifying internal associate type
/// like: `dyn TidAble<'a,Static=SomeType>` which make such trait object effectively useless.
///
/// Unsafe because safety of this crate relies on correctness of this trait implementation.
/// There are several safe ways to implement it:
///  - `type_id`/`tid` declarative macro
///  - `#[derive(Tid)]` derive macro
///  - impl_tid` attribute macro
// we need to have associate type because it allows TypeIdAdjuster to be a private type
// and allows to implement it for generic types
// it has lifetime and depends on Tid because it would be practically useless as a standalone trait
// because even though user would be able to get type id for more types,
// any action based on it would be unsound without checking on lifetimes
pub unsafe trait TidAble<'a>: Tid<'a> {
    /// Implementation detail
    #[doc(hidden)]
    type Static: ?Sized + Any;
}

/// Extension trait that contains actual downcasting methods.
///
/// Use methods from this trait only if `dyn Tid` was created directly from `T` for this particular `T`
///
/// If `Self` is `Sized` then any of those calls is optimized to no-op because both T and Self are known statically.
/// Useful if you have generic code that you want to behave differently depending on which
/// concrete type replaces type parameter. Usually there are better ways to do this like specialization,
/// but sometimes it can be the only way.
pub trait TidExt<'a>: Tid<'a> {
    /// Returns true if type behind self is equal to the type of T.
    fn is<T: Tid<'a>>(&self) -> bool {
        self.self_id() == T::id()
    }

    /// Attempts to downcast self to `T` behind reference
    fn downcast_ref<'b, T: Tid<'a>>(&'b self) -> Option<&'b T> {
        // Tid<'a> is implemented only for types with lifetime 'a
        // so we can safely cast type back because lifetime invariant is preserved.
        if self.is::<T>() {
            Some(unsafe { &*(self as *const _ as *const T) })
        } else {
            None
        }
    }

    /// Attempts to downcast self to `T` behind mutable reference
    fn downcast_mut<'b, T: Tid<'a>>(&'b mut self) -> Option<&'b mut T> {
        // see downcast_ref
        if self.is::<T>() {
            Some(unsafe { &mut *(self as *mut _ as *mut T) })
        } else {
            None
        }
    }

    /// Attempts to downcast self to `T` behind `Rc` pointer
    fn downcast_rc<T: Tid<'a>>(self: Rc<Self>) -> Result<Rc<T>, Rc<Self>> {
        if self.is::<T>() {
            unsafe { Ok(Rc::from_raw(Rc::into_raw(self) as *const _)) }
        } else {
            Err(self)
        }
    }

    /// Attempts to downcast self to `T` behind `Arc` pointer
    fn downcast_arc<T: Tid<'a>>(self: Arc<Self>) -> Result<Arc<T>, Arc<Self>> {
        if self.is::<T>() {
            unsafe { Ok(Arc::from_raw(Arc::into_raw(self) as *const _)) }
        } else {
            Err(self)
        }
    }

    /// Attempts to downcast self to `T` behind `Box` pointer
    fn downcast_box<T: Tid<'a>>(self: Box<Self>) -> Result<Box<T>, Box<Self>> {
        if self.is::<T>() {
            unsafe { Ok(Box::from_raw(Box::into_raw(self) as *mut _)) }
        } else {
            Err(self)
        }
    }

    /// Attempts to downcast owned `Self` to `T`,
    /// useful only in generic context as a workaround for specialization
    fn downcast_move<T: Tid<'a>>(self) -> Option<T>
    where
        Self: Sized,
    {
        if self.is::<T>() {
            // can't use `Option` trick here like with `Any`
            let this = core::mem::MaybeUninit::new(self);
            return Some(unsafe { core::mem::transmute_copy(&this) });
        }
        None
    }
}
impl<'a, X: ?Sized + Tid<'a>> TidExt<'a> for X {}

/// Methods here are implemented as an associated functions because otherwise
/// for one they will conflict with methods defined on `dyn Any` in stdlib,
/// for two they will be available on almost every type in the program causing confusing bugs and error messages
/// For example if you have `&Box<dyn Any>` and call `downcast_ref`, instead of failing or working on coerced `&dyn Any`
/// it would work with type id of `Box<dyn Any>` itself instead of the type behind `dyn Any`.
pub trait AnyExt: Any {
    /// Attempts to downcast this to `T` behind reference
    fn downcast_ref<T: Any>(this: &Self) -> Option<&T> {
        // Any is implemented only for types with lifetime 'a
        // so we can safely cast type back because lifetime invariant is preserved.
        if this.type_id() == TypeId::of::<T>() {
            Some(unsafe { &*(this as *const _ as *const T) })
        } else {
            None
        }
    }

    /// Attempts to downcast this to `T` behind mutable reference
    fn downcast_mut<T: Any>(this: &mut Self) -> Option<&mut T> {
        // see downcast_ref
        if (*this).type_id() == TypeId::of::<T>() {
            Some(unsafe { &mut *(this as *mut _ as *mut T) })
        } else {
            None
        }
    }

    /// Attempts to downcast this to `T` behind `Rc` pointer
    fn downcast_rc<T: Any>(this: Rc<Self>) -> Result<Rc<T>, Rc<Self>> {
        if this.type_id() == TypeId::of::<T>() {
            unsafe { Ok(Rc::from_raw(Rc::into_raw(this) as *const _)) }
        } else {
            Err(this)
        }
    }

    /// Attempts to downcast this to `T` behind `Arc` pointer
    fn downcast_arc<T: Any>(this: Arc<Self>) -> Result<Arc<T>, Arc<Self>> {
        if this.type_id() == TypeId::of::<T>() {
            unsafe { Ok(Arc::from_raw(Arc::into_raw(this) as *const _)) }
        } else {
            Err(this)
        }
    }

    /// Attempts to downcast this to `T` behind `Box` pointer
    fn downcast_box<T: Any>(this: Box<Self>) -> Result<Box<T>, Box<Self>> {
        if this.type_id() == TypeId::of::<T>() {
            unsafe { Ok(Box::from_raw(Box::into_raw(this) as *mut _)) }
        } else {
            Err(this)
        }
    }

    /// Attempts to downcast owned `Self` to `T`,
    /// useful only in generic context as a workaround for specialization
    fn downcast_move<T: Any>(this: Self) -> Option<T>
    where
        Self: Sized,
    {
        let temp = &mut Some(this) as &mut dyn Any;
        if let Some(temp) = AnyExt::downcast_mut::<Option<T>>(temp) {
            return Some(temp.take().unwrap());
        }
        None
    }
}
impl<T: ?Sized + Any> AnyExt for T {}

/// This trait indicates that this type can be converted to
/// trait object with typeid while preserving lifetime information.
/// Extends `Any` functionality for types with single lifetime
///
/// Use it only as a `dyn Tid<'a>` or as super trait when you need to create trait object.
/// In all other places use `TidAble<'a>`.
///
/// Lifetime here is necessary to make `dyn Tid<'a> + 'a` invariant over `'a`.
pub unsafe trait Tid<'a>: 'a {
    /// Returns type id of the type of `self`
    ///
    /// Note that returned type id is guaranteed to be different from provided by `Any`.
    /// It is necessary for the creation of `dyn Tid` from `dyn Any` to be sound.
    fn self_id(&self) -> TypeId;

    /// Returns type id of this type
    fn id() -> TypeId
    where
        Self: Sized;
}

unsafe impl<'a, T: ?Sized + TidAble<'a>> Tid<'a> for T {
    #[inline]
    fn self_id(&self) -> TypeId {
        adjust_id::<T::Static>()
    }

    #[inline]
    fn id() -> TypeId
    where
        Self: Sized,
    {
        adjust_id::<T::Static>()
    }
}

#[inline(always)]
fn adjust_id<T: ?Sized + Any>() -> TypeId {
    TypeId::of::<T>()
}

/// Returns type id of `T`
///
/// Use it only if `Tid::id()` is not enough when `T` is not sized.
#[inline]
pub fn typeid_of<'a, T: ?Sized + TidAble<'a>>() -> TypeId {
    adjust_id::<T::Static>()
}

impl<'a, T: Any> From<Box<T>> for Box<dyn Tid<'a> + 'a> {
    #[inline]
    fn from(f: Box<T>) -> Self {
        // TypeIdAdjuster is a transparent wrapper so it is sound
        unsafe { Box::from_raw(Box::into_raw(f) as *mut TypeIdAdjuster<T>) as _ }
    }
}

impl<'a: 'b, 'b, T: Any> From<&'b T> for &'b (dyn Tid<'a> + 'a) {
    #[inline]
    fn from(f: &'b T) -> Self {
        unsafe { &*(f as *const _ as *const TypeIdAdjuster<T> as *const _) }
    }
}

impl<'a: 'b, 'b, T: Any> From<&'b mut T> for &'b mut (dyn Tid<'a> + 'a) {
    #[inline]
    fn from(f: &'b mut T) -> Self {
        unsafe { &mut *(f as *mut _ as *mut TypeIdAdjuster<T> as *mut _) }
    }
}

// Reverse is possible only for 'static
// because otherwise even though user can't access type with lifetime because of different type id
// drop still can be called after the end of lifetime.
// impl Into<Box<dyn Any>> for Box<dyn Tid<'static>> {
//     fn into(self) -> Box<dyn Any> {
//         unsafe { core::mem::transmute(self) }
//     }
// }

//newtype wrapper to make `Any` types work with `dyn Tid`
#[repr(transparent)]
struct TypeIdAdjuster<T: ?Sized>(T);

tid! {impl<'a,T:'static> TidAble<'a> for TypeIdAdjuster<T> where T:?Sized}

impl<'a> dyn Tid<'a> + 'a {
    /// Tries to downcast `dyn Tid` to `T`
    ///
    /// Use it only if `dyn Tid` was created from concrete `T:Any` via `From` implementations.
    /// See examples how it does relate to other downcast methods
    ///
    /// ```rust
    /// # use std::any::Any;
    /// # use better_any::{Tid, TidAble, TidExt,tid};
    /// struct S;
    /// tid!(S);
    ///
    /// let a = &S;
    /// let from_any: &dyn Tid = a.into();
    /// assert!(from_any.downcast_any_ref::<S>().is_some());
    /// assert!(from_any.downcast_ref::<S>().is_none());
    ///
    /// let direct = &S as &dyn Tid;
    /// assert!(direct.downcast_any_ref::<S>().is_none());
    /// assert!(direct.downcast_ref::<S>().is_some());
    /// ```
    #[inline]
    pub fn downcast_any_ref<T: Any>(&self) -> Option<&T> {
        // SAFETY: just a transparent reference cast
        self.downcast_ref::<TypeIdAdjuster<T>>()
            .map(|x| unsafe { &*(x as *const _ as *const T) })
    }

    /// See `downcast_any_ref`
    #[inline]
    pub fn downcast_any_mut<T: Any>(&mut self) -> Option<&mut T> {
        // SAFETY: just a transparent reference cast
        self.downcast_mut::<TypeIdAdjuster<T>>()
            .map(|x| unsafe { &mut *(x as *mut _ as *mut T) })
    }

    /// See `downcast_any_ref`
    #[inline]
    pub fn downcast_any_box<T: Any>(self: Box<Self>) -> Result<Box<T>, Box<Self>> {
        // SAFETY: just a transparent reference cast
        self.downcast_box::<TypeIdAdjuster<T>>()
            .map(|x| unsafe { Box::from_raw(Box::into_raw(x) as *mut T) as _ })
    }
}

use std::cell::*;
use std::rc::*;
use std::sync::*;
tid!(impl<'a, T> TidAble<'a> for Box<T> where T:?Sized);
tid!(impl<'a, T> TidAble<'a> for Rc<T>);
tid!(impl<'a, T> TidAble<'a> for RefCell<T>);
tid!(impl<'a, T> TidAble<'a> for Cell<T>);
tid!(impl<'a, T> TidAble<'a> for Arc<T>);
tid!(impl<'a, T> TidAble<'a> for Mutex<T>);
tid!(impl<'a, T> TidAble<'a> for RwLock<T>);

// tid! {impl<'a, T> TidAble<'a> for Option<T>}
const _: () = {
    use core::marker::PhantomData;
    type __Alias<'a, T> = Option<T>;
    pub struct __TypeIdGenerator<'a, T: ?Sized>(PhantomData<&'a ()>, PhantomData<T>);
    unsafe impl<'a, T: TidAble<'a>> TidAble<'a> for __Alias<'a, T> {
        type Static = __TypeIdGenerator<'static, T::Static>;
    }
};

tid! {impl<'a, T> TidAble<'a> for Vec<T>}

tid! { impl<'a,T,E> TidAble<'a> for Result<T,E> }

tid! { impl<'a> TidAble<'a> for dyn Tid<'a> + 'a }

/// Main safe implementation interface of related unsafe traits
///
/// It uses syntax of regular Rust `impl` block but with parameters restricted enough to be sound.
/// In particular it is restricted to a single lifetime parameter in particular block.
/// and additional bounds must be in where clauses.
/// In trivial cases just type signature can be used.
///
/// ```rust
/// # use better_any::tid;
/// struct S;
/// tid!(S);
///
/// struct F<'a>(&'a str);
/// tid!(F<'a>);
///
/// struct Bar<'x,'y,X,Y>(&'x str,&'y str,X,Y);
/// tid!{ impl<'b,X,Y> TidAble<'b> for Bar<'b,'b,X,Y> }
///
/// trait Test<'a>{}
/// tid!{ impl<'b> TidAble<'b> for dyn Test<'b> + 'b }
/// ```
///
/// Implementation by default adds `TidAble<'a>` bound on all generic parameters.
/// This behavior can be opted out by specifying `'static` bound on corresponding type parameter.
/// Note that due to decl macro limitations it must be specified directly on type parameter
/// and **not** in where clauses:
/// ```rust
/// # use better_any::tid;
/// struct Test<'a,X:?Sized>(&'a str,Box<X>);
/// tid! { impl<'a,X:'static> Tid<'a> for Test<'a,X> where X:?Sized }
/// ```
///
#[macro_export]
macro_rules! tid {

    ($struct: ident) => {
        unsafe impl<'a> $crate::TidAble<'a> for $struct {
            type Static = $struct;
        }
    };
    ($struct: ident < $lt: lifetime >) => {
        unsafe impl<'a> $crate::TidAble<'a> for $struct<'a> {
            type Static = $struct<'static>;
        }
    };
    // no static parameters case
    (impl <$lt:lifetime $(,$param:ident)*> $tr:ident<$lt2:lifetime> for $($struct: tt)+ ) => {
        $crate::tid!{ inner impl <$lt $(,$param)* static> $tr<$lt2> for $($struct)+  }
    };

    //todo change macro to use attributes instead of 'static
    // inner submacro is used to check/fix/error on whether correct trait is being implemented
    (inner impl <$lt:lifetime $(,$param:ident)* static $( $static_param:ident)* > Tid<$lt2:lifetime> for $($struct: tt)+ ) => {
        $crate::tid!{ inner impl <$lt $(,$param)* static $( $static_param)*> TidAble<$lt2> for $($struct)+  }
    };
    (inner impl <$lt:lifetime $(,$param:ident)* static $( $static_param:ident)* > TidAble<$lt2:lifetime> for $($struct: tt)+ ) => {
        const _:() = {
            use core::marker::PhantomData;
            type __Alias<$lt $(,$param)* $(,$static_param)*>  = $crate::before_where!{ $($struct)+ };
            pub struct __TypeIdGenerator<$lt $(,$param:?Sized)* $(,$static_param:?Sized)*>
                (PhantomData<& $lt ()> $(,PhantomData<$param>)* $(,PhantomData<$static_param>)*);
            $crate::impl_block!{
                after where {  $($struct)+ }
                {unsafe impl<$lt $(,$param:$crate::TidAble<$lt>)* $(,$static_param: 'static)* > $crate::TidAble<$lt2> for __Alias<$lt $(,$param)* $(,$static_param)*>}

                {
                    type Static = __TypeIdGenerator<'static $(,$param::Static)* $(,$static_param)*>;
                }
            }
        };
    };
    (inner impl <$lt:lifetime $(,$param:ident)* static $( $static_param:ident)* > $tr:ident<$lt2:lifetime> for $($struct: tt)+ ) => {
        compile_error!{" wrong trait, should be TidAble or Tid "}
    };

    // temp submacro is used to separate 'static type parameters from other ones
    (temp $(,$param:ident)* static $(,$static_param:ident)* impl <$lt:lifetime , $token:ident : 'static $($tail: tt)+ ) => {
        $crate::tid!{ temp $(,$param)* static  $(,$static_param)* , $token  impl <$lt $($tail)+}
    };
    (temp $(,$param:ident)* static $(,$static_param:ident)* impl <$lt:lifetime , $token:ident $($tail: tt)+ ) => {
        $crate::tid!{ temp $(,$param)* ,$token static $(,$static_param)* impl <$lt $($tail)+ }
    };
    (temp $(,$param:ident)* static $(,$static_param:ident)* impl <$lt:lifetime> $($tail: tt)+ ) => {
        $crate::tid!{ inner impl <$lt $(,$param)* static $( $static_param)* > $($tail)+ }
    };
    // ( temp static  $($tail:tt)+ ) => {
    //     compile_error!{"invalid syntax"}
    // };
    ( impl $($tail: tt)+) => {
        $crate::tid!{ temp static impl $($tail)+ }
    };
}

struct Test<'a, X: ?Sized>(&'a str, Box<X>);
// tid! { impl < 'a    static X    > TidAble < 'a > for Test < 'a , X > where X : ? Sized  }
tid! { impl<'a,X:'static> TidAble<'a> for Test<'a,X> where X:?Sized }

#[doc(hidden)]
#[macro_export]
macro_rules! before_where {
    (inner { $($processed:tt)* } where     $($tokens:tt)* ) => { $($processed)* };
    (inner { $($processed:tt)* } $token:tt $($tokens:tt)* ) => {
        $crate::before_where!(inner { $($processed)* $token }  $($tokens)*)
    };
    (inner { $($processed:tt)* } ) => { $($processed)* };
    ($($tokens:tt)*) => {$crate::before_where!(inner {} $($tokens)*)};
}

//creates actual impl block while also extracting tokens after where
#[doc(hidden)]
#[macro_export]
macro_rules! impl_block {
    (
        after where {}
        {$($imp:tt)*}
        { $($block:tt)* }
    ) => {
        $($imp)*

        {
            $($block)*
        }
    };
    (
        after where { where $($bounds:tt)* }
        {$($imp:tt)*}
        { $($block:tt)* }
    ) => {
        $($imp)*
            where $($bounds)*
        {
            $($block)*
        }
    };
    (
        after where {$token:tt $($tokens:tt)*}
        {$($imp:tt)*}
        { $($block:tt)* }
    ) => {
        $crate::impl_block!{
            after where { $($tokens)*}
            {$($imp)*}
            { $($block)* }

        }
    };
}
// the logic behind this implementations is to connect Any with Tid somehow
// I would say that if T:Any there is no much need to implement Tid<'a> for T.
// because Any functionality already exists and `dyn Any` can be converted to `dyn Tid`.
// unfortunately there is no way to implement Tid<'a> for T:Any,
// which make impl<'a, T: Tid<'a>> Tid<'a> for &'a T {} almost useless
// because it wouldn't work even for &'a i32
// This way we don't require user to newtype wrapping simple references.
// And more complex types are usually not used as a type parameters directly.

tid! { impl<'a,T:'static> TidAble<'a> for &'a T }
tid! { impl<'a,T:'static> TidAble<'a> for &'a mut T }

/// Just an alias of `tid!` macro if someone considers that name to be more clear and for compatibility with previous versions.
///
/// ```rust
/// use better_any::type_id;
/// struct S;
/// type_id!(S);
/// struct F<'a>(&'a str);
/// type_id!(F<'a>);
/// ```
pub use tid as type_id;
// left it exported just to not needlessly break previous version code
// #[macro_export]
// macro_rules! type_id {
//     ($($tokens:tt)+) => { $crate::tid!{ $($tokens)+ } };
// }

/// unstable features that require nightly, use on your own risk
#[cfg(feature = "nightly")]
pub mod nightly;
