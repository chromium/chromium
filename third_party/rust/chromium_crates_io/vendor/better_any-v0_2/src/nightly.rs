use crate::Tid;
use std::any::Any;
use std::ops::CoerceUnsized;
use std::ptr::{DynMetadata, Pointee};
use std::rc::Rc;
use std::sync::Arc;

// todo support allocator for heap types
/// Implemented for types that can be converted to and from raw painter
pub trait IntoRawPtr {
    /// Contains lifetime of type if any.
    /// Required to enforce downcast pointer to have same lifetime as the input one.
    type Lifetime;
    /// Target of our pointer-like type
    type Pointee: ?Sized;

    /// Converts to raw pointer
    unsafe fn into_raw(self) -> *const Self::Pointee;
    /// Reconstruct Self from raw pointer
    unsafe fn from_raw(from: *const Self::Pointee) -> Self;
}

impl<T: ?Sized> IntoRawPtr for Box<T> {
    type Lifetime = ();
    type Pointee = T;

    unsafe fn into_raw(self) -> *const Self::Pointee {
        Box::into_raw(self)
    }

    unsafe fn from_raw(from: *const Self::Pointee) -> Self {
        Box::from_raw(from as *mut _)
    }
}

impl<T: ?Sized> IntoRawPtr for Rc<T> {
    type Lifetime = ();
    type Pointee = T;

    unsafe fn into_raw(self) -> *const Self::Pointee {
        Rc::into_raw(self)
    }

    unsafe fn from_raw(from: *const Self::Pointee) -> Self {
        Rc::from_raw(from)
    }
}

impl<T: ?Sized> IntoRawPtr for Arc<T> {
    type Lifetime = ();
    type Pointee = T;

    unsafe fn into_raw(self) -> *const Self::Pointee {
        Arc::into_raw(self)
    }

    unsafe fn from_raw(from: *const Self::Pointee) -> Self {
        Arc::from_raw(from)
    }
}

impl<'a, T: ?Sized> IntoRawPtr for &'a T {
    type Lifetime = &'a ();
    type Pointee = T;

    unsafe fn into_raw(self) -> *const Self::Pointee {
        self
    }

    unsafe fn from_raw(from: *const Self::Pointee) -> Self {
        &*from
    }
}

impl<'a, T: ?Sized> IntoRawPtr for &'a mut T {
    type Lifetime = &'a mut ();
    type Pointee = T;

    unsafe fn into_raw(self) -> *const Self::Pointee {
        self as *mut T as _
    }

    unsafe fn from_raw(from: *const Self::Pointee) -> Self {
        &mut *(from as *mut _)
    }
}

// tid!{impl<'a,X> TidAble<'a> for DynMetaData<X> where X:?Sized}

/// Helper trait to retrieve trait object type
pub trait DynMetadataType: Pointee<Metadata = DynMetadata<Self::Over>> {
    /// type inside of DynMetadata
    type Over: ?Sized + Pointee<Metadata = DynMetadata<Self::Over>>;
}

impl<T: ?Sized, X: ?Sized> DynMetadataType for X
where
    X: Pointee<Metadata = DynMetadata<T>>,
    T: Pointee<Metadata = DynMetadata<T>>,
{
    type Over = T;
}

fn get_callable_trait_object<T: ?Sized + DynMetadataType>(
    ptr: *const T,
) -> *const <T as DynMetadataType>::Over {
    let metadata = ptr.to_raw_parts().1;
    //SAFETY: currently there is no validity requirements for fat pointers that make data pointer and
    // vtable pointer in any way dependent.
    // Here both of these parts are valid independently which is enough for validity of resulting fat pointer
    core::ptr::from_raw_parts(&(), metadata)
}

/// Downcasts any kind of fat pointer type which vtable corresponds to a trait with `Tid` bound.
/// For example `Rc<RefCell<dyn Tid<'_>>>>` can be downcasted with this method
///
/// ```rust
/// # use better_any::nightly::{downcast_tid, DowncastExt};
/// # use better_any::{Tid,tid};
/// # use std::fmt::Debug;
/// struct Test(i32);
/// tid!(Test);
/// let a = Box::new(Test(5i32));
/// let any = a as Box<dyn Tid>;
/// let result: Box<Test> = downcast_tid(any).unwrap_or_else(|_| panic!("error"));
/// assert_eq!(5, result.0);
///```
pub fn downcast_tid<'a, From: IntoRawPtr, To: IntoRawPtr<Lifetime = From::Lifetime>>(
    f: From,
) -> Result<To, From>
where
    From::Pointee: Pointee + DynMetadataType,
    To::Pointee: Sized,
    // To: CoerceUnsized<From>, // required to make lifetimes of `To` and `From` to be the same
    *const To::Pointee: CoerceUnsized<*const From::Pointee>,
    <From::Pointee as DynMetadataType>::Over: Tid<'a>,
{
    let raw = unsafe { f.into_raw() };

    // get callable vtable for input type
    let vtable_only_pointer_from = unsafe { &*get_callable_trait_object(raw) };
    // get callable vtable for output type
    let vtable_only_pointer_to = unsafe {
        &*get_callable_trait_object(&() as *const () as *const To::Pointee as *const From::Pointee)
    };

    // self_id call does not access `&self`
    if vtable_only_pointer_from.self_id() == vtable_only_pointer_to.self_id() {
        unsafe { Ok(To::from_raw(raw as _)) }
    } else {
        Err(unsafe { From::from_raw(raw) })
    }
}

/// Downcasts any kind of fat pointer type which vtable corresponds to a trait with `Any` bound.
/// For example `Rc<RefCell<dyn Any>>>` can be downcasted with this method
///
///```rust
/// # use better_any::nightly::{downcast_any, DowncastExt};
/// # use std::any::Any;
/// # use std::fmt::Debug;
/// let a = 5i32;
/// let any = &a as &dyn Any;
/// let result: &i32 = downcast_any(any).unwrap();
/// assert_eq!(a, *result);
/// assert!(downcast_any::<_, &usize>(any).is_err());
///```
pub fn downcast_any<From: IntoRawPtr, To: IntoRawPtr<Lifetime = From::Lifetime>>(
    f: From,
) -> Result<To, From>
where
    From::Pointee: Pointee + DynMetadataType,
    To::Pointee: Sized,
    // To: CoerceUnsized<From>, // required to make lifetimes of `To` and `From` to be the same
    *const To::Pointee: CoerceUnsized<*const From::Pointee>,
    <From::Pointee as DynMetadataType>::Over: Any,
{
    let raw = unsafe { f.into_raw() };

    // get callable vtable for input type
    let vtable_only_pointer_from = unsafe { &*get_callable_trait_object(raw) };
    // get callable vtable for output type
    let vtable_only_pointer_to = unsafe {
        &*get_callable_trait_object(&() as *const () as *const To::Pointee as *const From::Pointee)
    };

    // self_id call does not access `&self`
    if vtable_only_pointer_from.type_id() == vtable_only_pointer_to.type_id() {
        unsafe { Ok(To::from_raw(raw as _)) }
    } else {
        Err(unsafe { From::from_raw(raw) })
    }
}

/// Most generic downcast methods with new nightly `ptr_metadata` api
///
/// Works on almost anything that have unsizing coercion.
/// In particular it can downcast `Arc<Mutex<dyn Any>>` to `Arc<Mutex<Concrete>>` without locking mutex.
/// Similar for `Rc<RefCell<dyn Trait>>`.
///
/// It is deliberately implemented only on trait objects.
pub trait DowncastExt: Sized + IntoRawPtr {
    /// Attempts to downcast `Self` which is some kind of compatible fat pointer type
    /// to `T` which is thin version to that pointer with concrete pointee type.
    ///
    /// ```rust
    /// # use better_any::nightly::{downcast_any, DowncastExt};
    /// # use std::any::Any;
    /// # use std::cell::{Cell, RefCell};
    /// # use std::fmt::Debug;
    /// # use std::rc::Rc;
    /// trait DebugAny: Debug + Any {}
    /// # impl<X: Debug + Any> DebugAny for X {}let rc = Rc::new(Cell::new(5i32));
    /// let debug_rc = rc.clone() as Rc<Cell<dyn DebugAny>>;
    /// let result: Rc<Cell<i32>> = debug_rc.clone().downcast_any().ok().unwrap();
    /// assert_eq!(rc.get(), result.get());
    /// assert!(debug_rc.downcast_any::<Rc<Cell<usize>>>().is_err());
    /// ```
    fn downcast_any<T>(self) -> Result<T, Self>
    where
        Self::Pointee: Pointee + DynMetadataType,
        T: IntoRawPtr<Lifetime = Self::Lifetime>,
        T::Pointee: Sized,
        *const T::Pointee: CoerceUnsized<*const Self::Pointee>,
        <Self::Pointee as DynMetadataType>::Over: Any,
    {
        downcast_any(self)
    }

    /// Same as `downcast_any` but for `Tid` types
    fn downcast_tid<'a, T: IntoRawPtr>(self) -> Result<T, Self>
    where
        Self::Pointee: Pointee + DynMetadataType,
        T: IntoRawPtr<Lifetime = Self::Lifetime>,
        T::Pointee: Sized,
        *const T::Pointee: CoerceUnsized<*const Self::Pointee>,
        <Self::Pointee as DynMetadataType>::Over: Tid<'a>,
    {
        downcast_tid(self)
    }
}

impl<T: IntoRawPtr> DowncastExt for T where T::Pointee: DynMetadataType {}

/// Checks that wrong lifetime doesn't work
/// ```rust,compile_fail
/// # use better_any::nightly::{downcast_any, DowncastExt};
/// # use std::any::Any;
/// # use std::fmt::Debug;
/// let a = 5i32;
/// let any = &a as &dyn Any;
/// let result: &'static i32 = downcast_any(any).unwrap();
///```
fn doc_test() {}
