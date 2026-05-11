/// [`Epoch`] represents the period of time the global epoch value stays the same.
///
/// The global epoch rotates four [`Epoch`] values in a range of `[0..3]` while the crate itself
/// functions correctly if the range is limited to `[0..2]`. The one additional state is useful for
/// users to determine whether a certain memory chunk can be deallocated or not by using values
/// returned from [`Guard::epoch`](crate::Guard::epoch), e.g., if an [`Owned`](crate::Owned) was
/// retired in [`Epoch`] `1`, then the [`Owned`](crate::Owned) will become completely unreachable in
/// [`Epoch`] `0`.
#[derive(Clone, Copy, Debug, Default, Eq, Ord, PartialEq, PartialOrd)]
pub struct Epoch {
    value: u8,
}

impl Epoch {
    /// This crates uses `4` epoch values.
    const NUM_EPOCHS: u8 = 4;

    /// Returns a future [`Epoch`] when the current readers will not be present.
    ///
    /// The current [`Epoch`] may lag behind the global epoch value by `1`, therefore this method
    /// returns an [`Epoch`] three epochs next to `self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use sdd::Epoch;
    ///
    /// let initial = Epoch::default();
    ///
    /// let next_generation = initial.next_generation();
    /// assert_eq!(next_generation, initial.next().next().next());
    /// ```
    #[inline]
    #[must_use]
    pub const fn next_generation(self) -> Epoch {
        self.prev()
    }

    /// Returns the next [`Epoch`] value.
    ///
    /// # Examples
    ///
    /// ```
    /// use sdd::Epoch;
    ///
    /// let initial = Epoch::default();
    ///
    /// let next = initial.next();
    /// assert!(initial < next);
    ///
    /// let next_next = next.next();
    /// assert!(next < next_next);
    /// ```
    #[inline]
    #[must_use]
    pub const fn next(self) -> Epoch {
        Epoch {
            value: (self.value + 1) % Self::NUM_EPOCHS,
        }
    }

    /// Returns the previous [`Epoch`] value.
    ///
    /// # Examples
    ///
    /// ```
    /// use sdd::Epoch;
    ///
    /// let initial = Epoch::default();
    ///
    /// let prev = initial.prev();
    /// assert!(initial < prev);
    ///
    /// let prev_prev = prev.prev();
    /// assert!(prev_prev < prev);
    /// ```
    #[inline]
    #[must_use]
    pub const fn prev(self) -> Epoch {
        Epoch {
            value: (self.value + Self::NUM_EPOCHS - 1) % Self::NUM_EPOCHS,
        }
    }

    /// Construct an [`Epoch`] from a [`u8`] value.
    #[inline]
    pub(super) const fn from_u8(value: u8) -> Epoch {
        Epoch { value }
    }
}

impl From<Epoch> for u8 {
    #[inline]
    fn from(epoch: Epoch) -> Self {
        epoch.value
    }
}
