// SPDX-License-Identifier: Apache-2.0 OR MIT

// Adapted from https://github.com/crossbeam-rs/crossbeam/blob/crossbeam-utils-0.8.21/crossbeam-utils/src/atomic/seq_lock_wide.rs.

include!("seq_lock_common.rs");

use crate::imp::core_atomic::AtomicU32;

pub(super) type State = u32;

/// A simple stamped lock.
///
/// The state is represented as two `AtomicU32`: `state_hi` for high bits and `state_lo` for low
/// bits.
pub(super) struct SeqLock {
    /// The high bits of the current state of the lock.
    state_hi: AtomicU32,

    /// The low bits of the current state of the lock.
    ///
    /// All bits except the least significant one hold the current stamp. When locked, the state_lo
    /// equals 1 and doesn't contain a valid stamp.
    state: AtomicU32,
}

impl SeqLock {
    #[inline]
    pub(super) const fn new() -> Self {
        Self { state_hi: AtomicU32::new(0), state: AtomicU32::new(0) }
    }

    /// If not locked, returns the current stamp.
    ///
    /// This method should be called before optimistic reads.
    #[inline]
    pub(super) fn optimistic_read(&self) -> Option<(State, State)> {
        // The acquire loads from `state_hi` and `state_lo` synchronize with the release stores in
        // `SeqLockWriteGuard::drop` and `SeqLockWriteGuard::abort`.
        //
        // As a consequence, we can make sure that (1) all writes within the era of `state_hi - 1`
        // happens before now; and therefore, (2) if `state_lo` is even, all writes within the
        // critical section of (`state_hi`, `state_lo`) happens before now.
        let state_hi = self.state_hi.load(Ordering::Acquire);
        let state_lo = self.state.load(Ordering::Acquire);
        if state_lo == LOCKED { None } else { Some((state_hi, state_lo)) }
    }

    /// Returns `true` if the current stamp is equal to `stamp`.
    ///
    /// This method should be called after optimistic reads to check whether they are valid. The
    /// argument `stamp` should correspond to the one returned by method `optimistic_read`.
    #[inline]
    pub(super) fn validate_read(&self, stamp: (State, State)) -> bool {
        // Thanks to the fence, if we're noticing any modification to the data at the critical
        // section of `(stamp.0, stamp.1)`, then the critical section's write of 1 to state_lo
        // should be visible.
        crate::fence(Ordering::Acquire);

        // So if `state_lo` coincides with `stamp.1`, then either (1) we're noticing no modification
        // to the data after the critical section of `(stamp.0, stamp.1)`, or (2) `state_lo` wrapped
        // around.
        //
        // If (2) is the case, the acquire ordering ensures we see the new value of `state_hi`.
        let state_lo = self.state.load(Ordering::Acquire);

        // If (2) is the case and `state_hi` coincides with `stamp.0`, then `state_hi` also wrapped
        // around, which we give up to correctly validate the read.
        let state_hi = self.state_hi.load(Ordering::Relaxed);

        // Except for the case that both `state_hi` and `state_lo` wrapped around, the following
        // condition implies that we're noticing no modification to the data after the critical
        // section of `(stamp.0, stamp.1)`.
        (state_hi, state_lo) == stamp
    }
}

impl SeqLockWriteGuard<'_> {
    #[inline]
    fn next_stamp(&self) -> State {
        let state_lo = self.state.wrapping_add(2);

        // Increase the high bits if the low bits wrap around.
        //
        // Release ordering for synchronizing with `optimistic_read`.
        if state_lo == 0 {
            let state_hi = self.lock.state_hi.load(Ordering::Relaxed);
            self.lock.state_hi.store(state_hi.wrapping_add(1), Ordering::Release);
        }

        state_lo
    }
}
