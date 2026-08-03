//! Tests for `Zeroize` impls on heap-allocated data structures

#![allow(clippy::std_instead_of_core, clippy::undocumented_unsafe_blocks)]

use std::alloc::{GlobalAlloc, Layout, System};
use zeroize::Zeroize;

// Allocator that ensures that deallocated data is zeroized.
struct ProxyAllocator;

unsafe impl GlobalAlloc for ProxyAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if layout.size() == 160 {
            for i in 0..layout.size() {
                let b = unsafe { core::ptr::read(ptr.add(i)) };
                assert_eq!(b, 0);
            }
        }

        unsafe { System.dealloc(ptr, layout) }
    }
}

#[global_allocator]
static PROXY_ALLOCATOR: ProxyAllocator = ProxyAllocator;

struct SecretBox<S: Zeroize>(Box<S>);

impl<S: Zeroize> SecretBox<S> {
    fn new(val: S) -> Self {
        Self(Box::new(val))
    }
}

impl<S: Zeroize> Drop for SecretBox<S> {
    fn drop(&mut self) {
        self.0.as_mut().zeroize();
    }
}

#[test]
fn secret_box_alloc_test() {
    let b1 = SecretBox::new([u128::MAX; 10]);
    core::hint::black_box(&b1);
    let b2 = SecretBox::new([u8::MAX; 160]);
    core::hint::black_box(&b2);
}

struct ObserveSecretBox<S: Default>(Box<S>);

impl<S: Default> ObserveSecretBox<S> {
    fn new(val: S) -> Self {
        Self(Box::new(val))
    }
}

impl<S: Default> Drop for ObserveSecretBox<S> {
    fn drop(&mut self) {
        *self.0 = Default::default();
        zeroize::optimization_barrier(&self);
    }
}

#[test]
fn observe_secret_box_alloc_test() {
    let b1 = ObserveSecretBox::new([u128::MAX; 10]);
    core::hint::black_box(&b1);
    let b2 = SecretBox::new([u8::MAX; 160]);
    core::hint::black_box(&b2);
}
