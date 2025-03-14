// Copyright 2023 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

//! Abstractions over raw pointers.

mod inner;
#[doc(hidden)]
pub mod invariant;
mod ptr;

#[doc(hidden)]
pub use invariant::{BecauseExclusive, BecauseImmutable, Read};
#[doc(hidden)]
pub use ptr::Ptr;

use crate::Unaligned;

/// A shorthand for a maybe-valid, maybe-aligned reference. Used as the argument
/// to [`TryFromBytes::is_bit_valid`].
///
/// [`TryFromBytes::is_bit_valid`]: crate::TryFromBytes::is_bit_valid
pub type Maybe<'a, T, Aliasing = invariant::Shared, Alignment = invariant::Unaligned> =
    Ptr<'a, T, (Aliasing, Alignment, invariant::Initialized)>;

/// A semi-user-facing wrapper type representing a maybe-aligned reference, for
/// use in [`TryFromBytes::is_bit_valid`].
///
/// [`TryFromBytes::is_bit_valid`]: crate::TryFromBytes::is_bit_valid
pub type MaybeAligned<'a, T, Aliasing = invariant::Shared, Alignment = invariant::Unaligned> =
    Ptr<'a, T, (Aliasing, Alignment, invariant::Valid)>;

// These methods are defined on the type alias, `MaybeAligned`, so as to bring
// them to the forefront of the rendered rustdoc for that type alias.
impl<'a, T, Aliasing, Alignment> MaybeAligned<'a, T, Aliasing, Alignment>
where
    T: 'a + ?Sized,
    Aliasing: invariant::Aliasing,
    Alignment: invariant::Alignment,
{
    /// Reads the value from `MaybeAligned`.
    #[must_use]
    #[inline]
    pub fn read_unaligned<R>(self) -> T
    where
        T: Copy,
        T: invariant::Read<Aliasing, R>,
    {
        // SAFETY: By invariant on `MaybeAligned`, `raw` contains
        // validly-initialized data for `T`. By `T: Read<Aliasing>`, we are
        // permitted to perform a read of `self`'s referent.
        unsafe { self.as_inner().read_unaligned() }
    }
}

impl<'a, T, Aliasing, Alignment> MaybeAligned<'a, T, Aliasing, Alignment>
where
    T: 'a + ?Sized,
    Aliasing: invariant::Reference,
    Alignment: invariant::Alignment,
{
    /// Views the value as an aligned reference.
    ///
    /// This is only available if `T` is [`Unaligned`].
    #[must_use]
    #[inline]
    pub fn unaligned_as_ref(self) -> &'a T
    where
        T: Unaligned,
    {
        self.bikeshed_recall_aligned().as_ref()
    }
}

/// Checks if the referent is zeroed.
pub(crate) fn is_zeroed<T, I>(ptr: Ptr<'_, T, I>) -> bool
where
    T: crate::Immutable + crate::KnownLayout,
    I: invariant::Invariants<Validity = invariant::Initialized>,
    I::Aliasing: invariant::Reference,
{
    ptr.as_bytes::<BecauseImmutable>().as_ref().iter().all(|&byte| byte == 0)
}
