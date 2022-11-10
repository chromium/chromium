use std::{marker::PhantomData, ptr::NonNull};

#[repr(transparent)]
/// A raw pointer that owns its pointee
pub(crate) struct Own<T>
where
    T: ?Sized,
{
    pub(crate) ptr: NonNull<T>,
}

unsafe impl<T> Send for Own<T> where T: ?Sized {}
unsafe impl<T> Sync for Own<T> where T: ?Sized {}

impl<T> Copy for Own<T> where T: ?Sized {}

impl<T> Clone for Own<T>
where
    T: ?Sized,
{
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Own<T>
where
    T: ?Sized,
{
    pub(crate) fn new(ptr: Box<T>) -> Self {
        Own {
            ptr: unsafe { NonNull::new_unchecked(Box::into_raw(ptr)) },
        }
    }

    pub(crate) fn cast<U: CastTo>(self) -> Own<U::Target> {
        Own {
            ptr: self.ptr.cast(),
        }
    }

    pub(crate) unsafe fn boxed(self) -> Box<T> {
        Box::from_raw(self.ptr.as_ptr())
    }

    pub(crate) fn by_ref<'a>(&self) -> Ref<'a, T> {
        Ref {
            ptr: self.ptr,
            lifetime: PhantomData,
        }
    }

    pub(crate) fn by_mut<'a>(self) -> Mut<'a, T> {
        Mut {
            ptr: self.ptr,
            lifetime: PhantomData,
        }
    }
}

#[allow(explicit_outlives_requirements)]
#[repr(transparent)]
/// A raw pointer that represents a shared borrow of its pointee
pub(crate) struct Ref<'a, T>
where
    T: ?Sized,
{
    pub(crate) ptr: NonNull<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> Copy for Ref<'a, T> where T: ?Sized {}

impl<'a, T> Clone for Ref<'a, T>
where
    T: ?Sized,
{
    fn clone(&self) -> Self {
        *self
    }
}

impl<'a, T> Ref<'a, T>
where
    T: ?Sized,
{
    pub(crate) fn new(ptr: &'a T) -> Self {
        Ref {
            ptr: NonNull::from(ptr),
            lifetime: PhantomData,
        }
    }

    pub(crate) fn from_raw(ptr: NonNull<T>) -> Self {
        Ref {
            ptr,
            lifetime: PhantomData,
        }
    }

    pub(crate) fn cast<U: CastTo>(self) -> Ref<'a, U::Target> {
        Ref {
            ptr: self.ptr.cast(),
            lifetime: PhantomData,
        }
    }

    pub(crate) fn by_mut(self) -> Mut<'a, T> {
        Mut {
            ptr: self.ptr,
            lifetime: PhantomData,
        }
    }

    pub(crate) fn as_ptr(self) -> *const T {
        self.ptr.as_ptr() as *const T
    }

    pub(crate) unsafe fn deref(self) -> &'a T {
        &*self.ptr.as_ptr()
    }
}

#[allow(explicit_outlives_requirements)]
#[repr(transparent)]
/// A raw pointer that represents a unique borrow of its pointee
pub(crate) struct Mut<'a, T>
where
    T: ?Sized,
{
    pub(crate) ptr: NonNull<T>,
    lifetime: PhantomData<&'a mut T>,
}

impl<'a, T> Copy for Mut<'a, T> where T: ?Sized {}

impl<'a, T> Clone for Mut<'a, T>
where
    T: ?Sized,
{
    fn clone(&self) -> Self {
        *self
    }
}

impl<'a, T> Mut<'a, T>
where
    T: ?Sized,
{
    pub(crate) fn cast<U: CastTo>(self) -> Mut<'a, U::Target> {
        Mut {
            ptr: self.ptr.cast(),
            lifetime: PhantomData,
        }
    }

    pub(crate) fn by_ref(self) -> Ref<'a, T> {
        Ref {
            ptr: self.ptr,
            lifetime: PhantomData,
        }
    }

    pub(crate) fn extend<'b>(self) -> Mut<'b, T> {
        Mut {
            ptr: self.ptr,
            lifetime: PhantomData,
        }
    }

    pub(crate) unsafe fn deref_mut(self) -> &'a mut T {
        &mut *self.ptr.as_ptr()
    }
}

impl<'a, T> Mut<'a, T> {
    pub(crate) unsafe fn read(self) -> T {
        self.ptr.as_ptr().read()
    }
}

pub(crate) trait CastTo {
    type Target;
}

impl<T> CastTo for T {
    type Target = T;
}
