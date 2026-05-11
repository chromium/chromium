use super::collectible::{Collectible, Link};
use super::collector::Collector;
use std::ops::Deref;
use std::ptr::{self, addr_of, NonNull};
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::{self, Relaxed};

/// [`RefCounted`] stores an instance of type `T`, and a union of a link to the next
/// [`Collectible`] or the reference counter.
pub(super) struct RefCounted<T> {
    instance: T,
    next_or_refcnt: Link,
}

impl<T> RefCounted<T> {
    /// Creates a new [`RefCounted`] that allows ownership sharing.
    #[inline]
    pub(super) fn new_shared(instance: T) -> NonNull<RefCounted<T>> {
        let boxed = Box::new(Self {
            instance,
            next_or_refcnt: Link::new_shared(),
        });
        unsafe { NonNull::new_unchecked(Box::into_raw(boxed)) }
    }

    /// Creates a new [`RefCounted`] that disallows reference counting.
    ///
    /// The reference counter field is never used until the instance is retired.
    #[inline]
    pub(super) fn new_unique(instance: T) -> NonNull<RefCounted<T>> {
        let boxed = Box::new(Self {
            instance,
            next_or_refcnt: Link::new_unique(),
        });
        unsafe { NonNull::new_unchecked(Box::into_raw(boxed)) }
    }

    /// Tries to add a strong reference to the underlying instance.
    ///
    /// `order` must be as strong as `Acquire` for the caller to correctly validate the newest
    /// state of the pointer.
    #[inline]
    pub(super) fn try_add_ref(&self, order: Ordering) -> bool {
        self.ref_cnt()
            .fetch_update(
                order,
                order,
                |r| {
                    if r % 2 == 1 {
                        Some(r + 2)
                    } else {
                        None
                    }
                },
            )
            .is_ok()
    }

    /// Returns a mutable reference to the instance if the number of owners is `1`.
    #[inline]
    pub(super) fn get_mut_shared(&mut self) -> Option<&mut T> {
        if self.ref_cnt().load(Relaxed) == 1 {
            Some(&mut self.instance)
        } else {
            None
        }
    }

    /// Returns a mutable reference to the instance if it is uniquely owned.
    #[inline]
    pub(super) fn get_mut_unique(&mut self) -> &mut T {
        debug_assert_eq!(self.ref_cnt().load(Relaxed), 0);
        &mut self.instance
    }

    /// Adds a strong reference to the underlying instance.
    #[inline]
    pub(super) fn add_ref(&self) {
        let mut current = self.ref_cnt().load(Relaxed);
        loop {
            debug_assert_eq!(current % 2, 1);
            debug_assert!(current <= usize::MAX - 2, "reference count overflow");
            match self
                .ref_cnt()
                .compare_exchange_weak(current, current + 2, Relaxed, Relaxed)
            {
                Ok(_) => break,
                Err(actual) => {
                    current = actual;
                }
            }
        }
    }

    /// Drops a strong reference to the underlying instance.
    ///
    /// Returns `true` if it the last reference was dropped.
    #[inline]
    pub(super) fn drop_ref(&self) -> bool {
        // It does not have to be a load-acquire as everything's synchronized via the global
        // epoch.
        let mut current = self.ref_cnt().load(Relaxed);
        loop {
            debug_assert_ne!(current, 0);
            let new = if current <= 1 { 0 } else { current - 2 };
            match self
                .ref_cnt()
                .compare_exchange_weak(current, new, Relaxed, Relaxed)
            {
                Ok(_) => break,
                Err(actual) => {
                    current = actual;
                }
            }
        }
        current == 1
    }

    /// Returns a pointer to the instance.
    #[inline]
    pub(super) fn inst_ptr(self_ptr: *const Self) -> *const T {
        if self_ptr.is_null() {
            ptr::null()
        } else {
            unsafe { addr_of!((*self_ptr).instance) }
        }
    }

    /// Returns a reference to its reference count.
    #[inline]
    pub(super) fn ref_cnt(&self) -> &AtomicUsize {
        self.next_or_refcnt.ref_cnt()
    }

    /// Passes a pointer to [`RefCounted`] to the garbage collector.
    #[inline]
    pub(super) fn pass_to_collector(ptr: *mut Self) {
        let dyn_mut_ptr: *mut dyn Collectible = ptr;
        #[allow(clippy::transmute_ptr_to_ptr)]
        let dyn_mut_ptr: *mut dyn Collectible = unsafe { std::mem::transmute(dyn_mut_ptr) };
        unsafe {
            Collector::collect(Collector::current(), dyn_mut_ptr);
        }
    }
}

impl<T> Deref for RefCounted<T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &Self::Target {
        &self.instance
    }
}

impl<T> Collectible for RefCounted<T> {
    #[inline]
    fn next_ptr(&self) -> Option<NonNull<dyn Collectible>> {
        self.next_or_refcnt.next_ptr()
    }

    #[inline]
    fn set_next_ptr(&self, next_ptr: Option<NonNull<dyn Collectible>>) {
        self.next_or_refcnt.set_next_ptr(next_ptr);
    }
}
