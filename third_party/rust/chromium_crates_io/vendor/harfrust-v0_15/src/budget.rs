//! Limits on work and allocations while deriving cached OpenType layout data.
//!
//! These are independent of the buffer's shaping limits. They only run while
//! building caches, including lazy builds shared across shaping calls. Once an
//! allowance is insufficient, the affected lookup is discarded and stays
//! skipped. Beyond the limits, the retained lookups can depend on use order.

use core::mem::size_of;
use core::sync::atomic::{AtomicUsize, Ordering};

/// A font's preprocessing allowance, shared by lazy cache initializers.
///
/// Repeated visits to aliased data consume work again. Allocations consume
/// their requested capacity, even if the resulting cache is later discarded.
/// Neither allowance is replenished between lookup builds or shaping calls.
#[derive(Debug)]
pub(crate) struct Budget {
    work: AtomicUsize,
    bytes: AtomicUsize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct Exhausted;

impl Budget {
    // Calibrated against all lookups in 3716 Google Fonts and the HarfBuzz
    // performance/monster font collections. The highest measured work and
    // allocation use were below 5% of these allowances, including Gulzar's
    // 3625-subtable lookup. Ceilings also bound padding with unused data.
    pub fn for_table_bytes(bytes: usize) -> Self {
        Self::new(
            bytes.saturating_mul(256).clamp(4 << 20, 128 << 20),
            bytes.saturating_mul(256).clamp(4 << 20, 128 << 20),
        )
    }

    pub const fn new(work: usize, bytes: usize) -> Self {
        Self {
            work: AtomicUsize::new(work),
            bytes: AtomicUsize::new(bytes),
        }
    }

    pub fn work(&self, visits: usize) -> Result<(), Exhausted> {
        Self::take(&self.work, visits)
    }

    pub fn alloc<T>(&self, count: usize) -> Result<(), Exhausted> {
        let bytes = count.checked_mul(size_of::<T>()).ok_or(Exhausted)?;
        Self::take(&self.bytes, bytes)
    }

    fn take(counter: &AtomicUsize, amount: usize) -> Result<(), Exhausted> {
        if amount == 0 {
            return Ok(());
        }
        // `try_update` is newer than the MSRV; `fetch_update` is deprecated.
        let mut remaining = counter.load(Ordering::Relaxed);
        loop {
            let next = remaining.checked_sub(amount).ok_or(Exhausted)?;
            match counter.compare_exchange_weak(
                remaining,
                next,
                Ordering::Relaxed,
                Ordering::Relaxed,
            ) {
                Ok(_) => return Ok(()),
                Err(actual) => remaining = actual,
            }
        }
    }

    #[cfg(test)]
    pub fn remaining(&self) -> (usize, usize) {
        (
            self.work.load(Ordering::Relaxed),
            self.bytes.load(Ordering::Relaxed),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allowances_are_independent_and_cannot_wrap() {
        let budget = Budget::new(3, 8);
        assert_eq!(budget.work(3), Ok(()));
        assert_eq!(budget.work(1), Err(Exhausted));
        assert_eq!(budget.alloc::<u32>(2), Ok(()));
        assert_eq!(budget.alloc::<u32>(1), Err(Exhausted));
        assert_eq!(budget.alloc::<u32>(usize::MAX), Err(Exhausted));
        assert_eq!(budget.remaining(), (0, 0));
    }

    #[test]
    fn size_scaling_is_bounded() {
        assert_eq!(Budget::for_table_bytes(0).remaining(), (4 << 20, 4 << 20));
        assert_eq!(
            Budget::for_table_bytes(usize::MAX).remaining(),
            (128 << 20, 128 << 20)
        );
    }

    #[test]
    #[cfg(feature = "std")]
    fn concurrent_builders_share_the_allowance() {
        let budget = Budget::new(1000, 1000);
        let spent = std::thread::scope(|scope| {
            let handles: Vec<_> = (0..8)
                .map(|_| {
                    scope.spawn(|| {
                        let mut spent = 0;
                        while budget.work(1).is_ok() {
                            spent += 1;
                        }
                        spent
                    })
                })
                .collect();
            handles
                .into_iter()
                .map(|h| h.join().unwrap())
                .sum::<usize>()
        });
        assert_eq!(spent, 1000);
        assert_eq!(budget.remaining(), (0, 1000));
    }
}
