//! Tests for `Zeroize` impls on heap-allocated data structures

use core::{
    alloc::{GlobalAlloc, Layout},
    ptr,
    sync::atomic::{AtomicPtr, Ordering::Relaxed},
};
use zeroize::Zeroize;

use std::alloc::System;

static REG_PTR: AtomicPtr<u8> = AtomicPtr::new(ptr::null_mut());

// Allocator that ensures that allocation registered in `REG_PTR` is zeroized.
struct ProxyAllocator;

#[allow(clippy::undocumented_unsafe_blocks)]
unsafe impl GlobalAlloc for ProxyAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if ptr == REG_PTR.load(Relaxed) {
            for i in 0..layout.size() {
                let b = unsafe { ptr::read(ptr.add(i)) };
                assert_eq!(b, 0);
            }
            REG_PTR.store(ptr::null_mut(), Relaxed);
        }

        unsafe { System.dealloc(ptr, layout) }
    }
}

#[global_allocator]
static PROXY_ALLOCATOR: ProxyAllocator = ProxyAllocator;

struct SecretBox<S: Zeroize>(Box<S>);

impl<S: Zeroize> SecretBox<S> {
    fn new(val: S) -> Self {
        let mut b = Box::new(val);
        // TODO(MSRV-1.98): use `Box::as_mut_ptr`
        let p: *mut S = &raw mut *b;
        REG_PTR.store(p.cast(), Relaxed);
        Self(b)
    }
}

impl<S: Zeroize> Drop for SecretBox<S> {
    fn drop(&mut self) {
        self.0.as_mut().zeroize();
    }
}

struct ObserveSecretBox<S: Default>(Box<S>);

impl<S: Default> ObserveSecretBox<S> {
    fn new(val: S) -> Self {
        let mut b = Box::new(val);
        // TODO(MSRV-1.98): use `Box::as_mut_ptr`
        let p: *mut S = &raw mut *b;
        REG_PTR.store(p.cast(), Relaxed);
        Self(b)
    }
}

impl<S: Default> Drop for ObserveSecretBox<S> {
    fn drop(&mut self) {
        *self.0 = Default::default();
        zeroize::optimization_barrier(&self);
    }
}

#[test]
fn proxy_alloc_test() {
    let b1 = SecretBox::new([u128::MAX; 10]);
    core::hint::black_box(&b1);
    drop(b1);

    let b2 = SecretBox::new([u8::MAX; 160]);
    core::hint::black_box(&b2);
    drop(b2);

    let b3 = ObserveSecretBox::new([u128::MAX; 10]);
    core::hint::black_box(&b3);
    drop(b3);

    let b4 = SecretBox::new([u8::MAX; 160]);
    core::hint::black_box(&b4);
    drop(b4);
}
