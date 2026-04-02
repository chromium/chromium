mod utils;

use core::sync::atomic::Ordering;
#[cfg(target_has_atomic = "8")]
use core::sync::atomic::{AtomicBool, AtomicI8, AtomicU8};
#[cfg(target_has_atomic = "16")]
use core::sync::atomic::{AtomicI16, AtomicU16};
#[cfg(target_has_atomic = "32")]
use core::sync::atomic::{AtomicI32, AtomicU32};
#[cfg(target_has_atomic = "64")]
use core::sync::atomic::{AtomicI64, AtomicU64};
#[cfg(target_has_atomic = "ptr")]
use core::sync::atomic::{AtomicIsize, AtomicUsize};
use utils::the_same_with_comparer;

#[test]
fn test_atomic_commons() {
    #[cfg(target_has_atomic = "8")]
    the_same_with_comparer(AtomicBool::new(true), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "8")]
    the_same_with_comparer(AtomicBool::new(false), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "8")]
    the_same_with_comparer(AtomicU8::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "16")]
    the_same_with_comparer(AtomicU16::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "32")]
    the_same_with_comparer(AtomicU32::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "64")]
    the_same_with_comparer(AtomicU64::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "ptr")]
    the_same_with_comparer(AtomicUsize::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "8")]
    the_same_with_comparer(AtomicI8::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "16")]
    the_same_with_comparer(AtomicI16::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "32")]
    the_same_with_comparer(AtomicI32::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "64")]
    the_same_with_comparer(AtomicI64::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
    #[cfg(target_has_atomic = "ptr")]
    the_same_with_comparer(AtomicIsize::new(0), |a, b| {
        a.load(Ordering::SeqCst) == b.load(Ordering::SeqCst)
    });
}
