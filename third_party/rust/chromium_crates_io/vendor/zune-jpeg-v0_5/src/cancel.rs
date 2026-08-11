/*
 * Copyright (c) 2026.
 *
 * This software is free software; You can redistribute it or modify it under terms of the MIT, Apache License or Zlib license
 */

//! Optional cooperative cancellation for long-running decodes.
//!
//! [`CancelCheck`] is a minimal, dependency-free trait the decoder polls at
//! coarse intervals (roughly once per MCU row) to decide whether to stop. Any
//! `Fn() -> bool` that is `Send + Sync` implements it, and [`NeverCancel`] costs
//! nothing to poll, so the check can be threaded through internals
//! unconditionally. Its shape is modelled on the `enough` crate's `Stop` trait
//! (<https://github.com/imazen/enough/blob/main/ZERO-DEP.md>).

use alloc::sync::Arc;

/// How many MCUs are decoded between polls of the cancel check.
///
/// At up to 16x16 pixels per MCU this bounds the work between polls to about
/// a quarter megapixel while keeping the check off the per-MCU path.
pub(crate) const CANCEL_POLL_INTERVAL_MCUS: usize = 1024;

/// A cooperative cancellation check, polled periodically during a long decode.
///
/// Set one with [`JpegDecoder::set_cancel`](crate::JpegDecoder::set_cancel);
/// when it fires, decoding returns
/// [`DecodeErrors::Cancelled`](crate::errors::DecodeErrors::Cancelled).
///
/// Any `Fn() -> bool` that is `Send + Sync` implements `CancelCheck`, so the
/// common case is a closure over an `Arc<AtomicBool>`, a channel, a deadline,
/// or an async cancellation token — the crate need not know about any of them.
/// Use [`NeverCancel`] when no cancellation is wanted; it is a zero-cost no-op
/// and the decoder's default.
pub trait CancelCheck: Send + Sync {
    /// Returns `true` to cancel decoding as soon as possible.
    ///
    /// Polled at coarse intervals, so it may be called many times during one
    /// decode — keep it cheap.
    fn is_cancelled(&self) -> bool;

    /// Returns `false` if this check can never fire, letting the decoder skip
    /// it entirely. The default is `true`.
    #[inline]
    fn may_cancel(&self) -> bool {
        true
    }
}

/// A [`CancelCheck`] that never cancels: a zero-cost opt-out of cooperative
/// cancellation, and the decoder's default.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct NeverCancel;

impl CancelCheck for NeverCancel {
    #[inline(always)]
    fn is_cancelled(&self) -> bool {
        false
    }

    #[inline(always)]
    fn may_cancel(&self) -> bool {
        false
    }
}

impl<F: Fn() -> bool + Send + Sync> CancelCheck for F {
    #[inline]
    fn is_cancelled(&self) -> bool {
        self()
    }
}

/// A stack-local throttle over the decoder's stored [`CancelCheck`]: forwards to
/// the underlying check on every `interval`-th call and returns `false` the rest
/// of the time, so a hot loop can poll once per iteration with a clean
/// `if cancel.is_cancelled()` while the user's closure runs only periodically.
///
/// It owns a clone of the decoder's `Arc<dyn CancelCheck>` (one refcount bump),
/// so it is independent of the decoder and can live in a loop body that also
/// borrows `&mut self`. When no check is set it holds `None`, so polling is a
/// single predicted branch.
pub(crate) struct Debounced {
    check:     Option<Arc<dyn CancelCheck>>,
    interval:  usize,
    countdown: usize
}

impl Debounced {
    pub(crate) fn new(check: Option<Arc<dyn CancelCheck>>, interval: usize) -> Debounced {
        Debounced {
            check:     check.filter(|c| c.may_cancel()),
            interval:  interval.max(1),
            countdown: 0
        }
    }

    /// Returns true if decoding should be cancelled. Consults the real check on
    /// the first call and then every `interval`-th call; returns `false`
    /// (cheaply) in between.
    #[inline]
    pub(crate) fn is_cancelled(&mut self) -> bool {
        let Some(check) = &self.check else {
            return false;
        };
        if self.countdown == 0 {
            self.countdown = self.interval - 1;
            check.is_cancelled()
        } else {
            self.countdown -= 1;
            false
        }
    }
}
