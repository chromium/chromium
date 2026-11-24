//! Generating UUIDs from timestamps.
//!
//! Timestamps are used in a few UUID versions as a source of decentralized
//! uniqueness (as in versions 1 and 6), and as a way to enable sorting (as
//! in versions 6 and 7). Timestamps aren't encoded the same way by all UUID
//! versions so this module provides a single [`Timestamp`] type that can
//! convert between them.
//!
//! # Timestamp representations in UUIDs
//!
//! Versions 1 and 6 UUIDs use a bespoke timestamp that consists of the
//! number of 100ns ticks since `1582-10-15 00:00:00`, along with
//! a counter value to avoid duplicates.
//!
//! Version 7 UUIDs use a more standard timestamp that consists of the
//! number of millisecond ticks since the Unix epoch (`1970-01-01 00:00:00`).
//!
//! # References
//!
//! * [UUID Version 1 in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-5.1)
//! * [UUID Version 7 in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-5.7)
//! * [Timestamp Considerations in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-6.1)

use core::cmp;

use crate::Uuid;

/// The number of 100 nanosecond ticks between the RFC 9562 epoch
/// (`1582-10-15 00:00:00`) and the Unix epoch (`1970-01-01 00:00:00`).
pub const UUID_TICKS_BETWEEN_EPOCHS: u64 = 0x01B2_1DD2_1381_4000;

/// A timestamp that can be encoded into a UUID.
///
/// This type abstracts the specific encoding, so versions 1, 6, and 7
/// UUIDs can both be supported through the same type, even
/// though they have a different representation of a timestamp.
///
/// # References
///
/// * [Timestamp Considerations in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-6.1)
/// * [UUID Generator States in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-6.3)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Timestamp {
    unix_nanos: u128,
    counter: u128,
    usable_counter_bits: u8,
}

impl Timestamp {
    /// Get a timestamp representing the current system time and up to a 128-bit counter.
    ///
    /// This method defers to the standard library's `SystemTime` type.
    #[cfg(feature = "std")]
    pub fn now(context: impl ClockSequence<Output = impl Into<u128>>) -> Self {
        let (seconds, subsec_nanos) = nanos_to_parts(now());

        let (counter, seconds, subsec_nanos) =
            context.generate_timestamp_sequence(seconds, subsec_nanos);
        let counter = counter.into();
        let usable_counter_bits = context.usable_bits() as u8;

        Timestamp {
            unix_nanos: parts_to_nanos(seconds, subsec_nanos),
            counter,
            usable_counter_bits,
        }
    }

    /// Construct a `Timestamp` from the number of 100 nanosecond ticks since 00:00:00.00,
    /// 15 October 1582 (the date of Gregorian reform to the Christian calendar) and a 14-bit
    /// counter, as used in versions 1 and 6 UUIDs.
    ///
    /// # Overflow
    ///
    /// If conversion from RFC 9562 ticks to the internal timestamp format would overflow
    /// it will wrap.
    pub const fn from_gregorian_time(ticks: u64, counter: u16) -> Self {
        let (seconds, subsec_nanos) = Self::gregorian_to_unix(ticks);

        Timestamp {
            unix_nanos: parts_to_nanos(seconds, subsec_nanos),
            counter: counter as u128,
            usable_counter_bits: 14,
        }
    }

    /// Construct a `Timestamp` from a Unix timestamp and up to a 128-bit counter, as used in version 7 UUIDs.
    pub const fn from_unix_time(
        seconds: u64,
        subsec_nanos: u32,
        counter: u128,
        usable_counter_bits: u8,
    ) -> Self {
        Timestamp {
            unix_nanos: parts_to_nanos(seconds, subsec_nanos),
            counter,
            usable_counter_bits,
        }
    }

    /// Construct a `Timestamp` from a Unix timestamp and up to a 128-bit counter, as used in version 7 UUIDs.
    pub fn from_unix(
        context: impl ClockSequence<Output = impl Into<u128>>,
        seconds: u64,
        subsec_nanos: u32,
    ) -> Self {
        let (counter, seconds, subsec_nanos) =
            context.generate_timestamp_sequence(seconds, subsec_nanos);
        let counter = counter.into();
        let usable_counter_bits = context.usable_bits() as u8;

        Timestamp {
            unix_nanos: parts_to_nanos(seconds, subsec_nanos),
            counter,
            usable_counter_bits,
        }
    }

    /// Get the value of the timestamp as the number of 100 nanosecond ticks since 00:00:00.00,
    /// 15 October 1582 and a 14-bit counter, as used in versions 1 and 6 UUIDs.
    ///
    /// # Overflow
    ///
    /// If conversion from the internal timestamp format to ticks would overflow
    /// then it will wrap.
    ///
    /// If the internal counter is wider than 14 bits then it will be truncated to 14 bits.
    pub const fn to_gregorian(&self) -> (u64, u16) {
        let (seconds, subsec_nanos) = nanos_to_parts(self.unix_nanos);

        (
            Self::unix_to_gregorian_ticks(seconds, subsec_nanos),
            (self.counter as u16) & 0x3FFF,
        )
    }

    // NOTE: This method is not public; the usable counter bits are lost in a version 7 UUID
    // so can't be reliably recovered.
    #[cfg(feature = "v7")]
    pub(crate) const fn counter(&self) -> (u128, u8) {
        (self.counter, self.usable_counter_bits)
    }

    /// Get the value of the timestamp as a Unix timestamp, as used in version 7 UUIDs.
    pub const fn to_unix(&self) -> (u64, u32) {
        nanos_to_parts(self.unix_nanos)
    }

    const fn unix_to_gregorian_ticks(seconds: u64, nanos: u32) -> u64 {
        UUID_TICKS_BETWEEN_EPOCHS
            .wrapping_add(seconds.wrapping_mul(10_000_000))
            .wrapping_add(nanos as u64 / 100)
    }

    const fn gregorian_to_unix(ticks: u64) -> (u64, u32) {
        (
            ticks.wrapping_sub(UUID_TICKS_BETWEEN_EPOCHS) / 10_000_000,
            (ticks.wrapping_sub(UUID_TICKS_BETWEEN_EPOCHS) % 10_000_000) as u32 * 100,
        )
    }
}

#[doc(hidden)]
impl Timestamp {
    #[deprecated(
        since = "1.10.0",
        note = "use `Timestamp::from_gregorian_time(ticks, counter)`"
    )]
    pub const fn from_rfc4122(ticks: u64, counter: u16) -> Self {
        Timestamp::from_gregorian_time(ticks, counter)
    }

    #[deprecated(since = "1.10.0", note = "use `Timestamp::to_gregorian()`")]
    pub const fn to_rfc4122(&self) -> (u64, u16) {
        self.to_gregorian()
    }

    #[deprecated(
        since = "1.2.0",
        note = "`Timestamp::to_unix_nanos()` is deprecated and will be removed: use `Timestamp::to_unix()`"
    )]
    pub const fn to_unix_nanos(&self) -> u32 {
        panic!("`Timestamp::to_unix_nanos()` is deprecated and will be removed: use `Timestamp::to_unix()`")
    }

    #[deprecated(
        since = "1.23.0",
        note = "use `Timestamp::from_gregorian_time(ticks, counter)`"
    )]
    pub const fn from_gregorian(ticks: u64, counter: u16) -> Self {
        Timestamp::from_gregorian_time(ticks, counter)
    }
}

#[cfg(feature = "std")]
impl TryFrom<std::time::SystemTime> for Timestamp {
    type Error = crate::Error;

    /// Perform the conversion.
    ///
    /// This method will fail if the system time is earlier than the Unix Epoch.
    /// On some platforms it may panic instead.
    fn try_from(st: std::time::SystemTime) -> Result<Self, Self::Error> {
        let dur = st.duration_since(std::time::UNIX_EPOCH).map_err(|_| {
            crate::Error(crate::error::ErrorKind::InvalidSystemTime(
                "unable to convert the system tie into a Unix timestamp",
            ))
        })?;

        Ok(Self::from_unix_time(
            dur.as_secs(),
            dur.subsec_nanos(),
            0,
            0,
        ))
    }
}

#[cfg(feature = "std")]
impl From<Timestamp> for std::time::SystemTime {
    /// Perform the conversion.
    ///
    /// If the conversion would fail, an undefined `SystemTime` will be returned instead.
    /// This can happen if the `Timestamp` would overflow the max value allowed by `SystemTime` on the target platform.
    /// Use `TryFrom` to catch conversion failures and handle them explicitly.
    fn from(ts: Timestamp) -> Self {
        let (seconds, subsec_nanos) = ts.to_unix();

        // NOTE: The actual value on overflow is undefined and may change
        // See: https://github.com/rust-lang/rust/issues/151199
        std::time::Duration::from_secs(seconds)
            .checked_add(std::time::Duration::from_nanos(subsec_nanos as u64))
            .and_then(|dur| Self::UNIX_EPOCH.checked_add(dur))
            .unwrap_or(std::time::SystemTime::UNIX_EPOCH)
    }
}

pub(crate) const fn encode_gregorian_timestamp(
    ticks: u64,
    counter: u16,
    node_id: &[u8; 6],
) -> Uuid {
    let time_low = (ticks & 0xFFFF_FFFF) as u32;
    let time_mid = ((ticks >> 32) & 0xFFFF) as u16;
    let time_high_and_version = (((ticks >> 48) & 0x0FFF) as u16) | (1 << 12);

    let mut d4 = [0; 8];

    d4[0] = (((counter & 0x3F00) >> 8) as u8) | 0x80;
    d4[1] = (counter & 0xFF) as u8;
    d4[2] = node_id[0];
    d4[3] = node_id[1];
    d4[4] = node_id[2];
    d4[5] = node_id[3];
    d4[6] = node_id[4];
    d4[7] = node_id[5];

    Uuid::from_fields(time_low, time_mid, time_high_and_version, &d4)
}

pub(crate) const fn decode_gregorian_timestamp(uuid: &Uuid) -> (u64, u16) {
    let bytes = uuid.as_bytes();

    let ticks: u64 = ((bytes[6] & 0x0F) as u64) << 56
        | (bytes[7] as u64) << 48
        | (bytes[4] as u64) << 40
        | (bytes[5] as u64) << 32
        | (bytes[0] as u64) << 24
        | (bytes[1] as u64) << 16
        | (bytes[2] as u64) << 8
        | (bytes[3] as u64);

    let counter: u16 = ((bytes[8] & 0x3F) as u16) << 8 | (bytes[9] as u16);

    (ticks, counter)
}

pub(crate) const fn encode_sorted_gregorian_timestamp(
    ticks: u64,
    counter: u16,
    node_id: &[u8; 6],
) -> Uuid {
    let time_high = ((ticks >> 28) & 0xFFFF_FFFF) as u32;
    let time_mid = ((ticks >> 12) & 0xFFFF) as u16;
    let time_low_and_version = ((ticks & 0x0FFF) as u16) | (0x6 << 12);

    let mut d4 = [0; 8];

    d4[0] = (((counter & 0x3F00) >> 8) as u8) | 0x80;
    d4[1] = (counter & 0xFF) as u8;
    d4[2] = node_id[0];
    d4[3] = node_id[1];
    d4[4] = node_id[2];
    d4[5] = node_id[3];
    d4[6] = node_id[4];
    d4[7] = node_id[5];

    Uuid::from_fields(time_high, time_mid, time_low_and_version, &d4)
}

pub(crate) const fn decode_sorted_gregorian_timestamp(uuid: &Uuid) -> (u64, u16) {
    let bytes = uuid.as_bytes();

    let ticks: u64 = ((bytes[0]) as u64) << 52
        | (bytes[1] as u64) << 44
        | (bytes[2] as u64) << 36
        | (bytes[3] as u64) << 28
        | (bytes[4] as u64) << 20
        | (bytes[5] as u64) << 12
        | ((bytes[6] & 0xF) as u64) << 8
        | (bytes[7] as u64);

    let counter: u16 = ((bytes[8] & 0x3F) as u16) << 8 | (bytes[9] as u16);

    (ticks, counter)
}

pub(crate) const fn encode_unix_timestamp_millis(
    millis: u64,
    counter_random_bytes: &[u8; 10],
) -> Uuid {
    let millis_high = ((millis >> 16) & 0xFFFF_FFFF) as u32;
    let millis_low = (millis & 0xFFFF) as u16;

    let counter_random_version = (counter_random_bytes[1] as u16
        | ((counter_random_bytes[0] as u16) << 8) & 0x0FFF)
        | (0x7 << 12);

    let mut d4 = [0; 8];

    d4[0] = (counter_random_bytes[2] & 0x3F) | 0x80;
    d4[1] = counter_random_bytes[3];
    d4[2] = counter_random_bytes[4];
    d4[3] = counter_random_bytes[5];
    d4[4] = counter_random_bytes[6];
    d4[5] = counter_random_bytes[7];
    d4[6] = counter_random_bytes[8];
    d4[7] = counter_random_bytes[9];

    Uuid::from_fields(millis_high, millis_low, counter_random_version, &d4)
}

pub(crate) const fn decode_unix_timestamp_millis(uuid: &Uuid) -> u64 {
    let bytes = uuid.as_bytes();

    let millis: u64 = (bytes[0] as u64) << 40
        | (bytes[1] as u64) << 32
        | (bytes[2] as u64) << 24
        | (bytes[3] as u64) << 16
        | (bytes[4] as u64) << 8
        | (bytes[5] as u64);

    millis
}

#[cfg(all(
    feature = "std",
    feature = "js",
    all(target_family = "wasm", any(target_os = "unknown", target_os = "none"))
))]
fn now() -> u128 {
    use wasm_bindgen::prelude::*;

    #[wasm_bindgen]
    extern "C" {
        // NOTE: This signature works around https://bugzilla.mozilla.org/show_bug.cgi?id=1787770
        #[wasm_bindgen(js_namespace = Date, catch)]
        fn now() -> Result<f64, JsValue>;
    }

    let now = now().unwrap_throw();

    (now * 1_000_000.0) as u128
}

#[cfg(all(
    feature = "std",
    not(miri),
    any(
        not(feature = "js"),
        not(all(target_family = "wasm", any(target_os = "unknown", target_os = "none")))
    )
))]
fn now() -> u128 {
    let dur = std::time::SystemTime::UNIX_EPOCH.elapsed().expect(
        "Getting elapsed time since UNIX_EPOCH. If this fails, we've somehow violated causality",
    );

    dur.as_nanos()
}

#[cfg(all(feature = "std", miri))]
fn now() -> u128 {
    use std::{sync::Mutex, time::Duration};

    static TS: Mutex<u128> = Mutex::new(0);

    let mut ts = TS.lock().unwrap();
    *ts += 1;
    *ts
}

/**
Convert a 128bit nanosecond value into a pair of 64bit seconds, and 32bit subsecond nanos.

The 32bit subsecond nano value is normalized to be less than `1_000_000_000` wherever the 64bit
seconds can hold the carry. Above `(u64::MAX, 999_999_999)` the remainder is left in the subsecond
nanos instead, so every value produced by `parts_to_nanos` converts back exactly.

Only values beyond `(u64::MAX, u32::MAX)` saturate to it. A `Timestamp` never holds one.
*/
const fn nanos_to_parts(nanos: u128) -> (u64, u32) {
    const MAX_NANOS: u128 = parts_to_nanos(u64::MAX, u32::MAX);

    if nanos > MAX_NANOS {
        return (u64::MAX, u32::MAX);
    }

    let secs = nanos / 1_000_000_000;

    if secs > u64::MAX as u128 {
        // Seconds can't take the carry, so leave the rest in the subsecond nanos (at most `u32::MAX`)
        (u64::MAX, (nanos - u64::MAX as u128 * 1_000_000_000) as u32)
    } else {
        (secs as u64, (nanos % 1_000_000_000) as u32)
    }
}

/**
Convert a potentially un-normalized part of 64bit seconds and 32bit subsecond nanos to a single
128bit number of nanoseconds.

This function will never overflow. `subsec_nanos` isn't required to be less than `1_000_000_000`.
*/
const fn parts_to_nanos(secs: u64, subsec_nanos: u32) -> u128 {
    (secs as u128 * 1_000_000_000) + subsec_nanos as u128
}

/// A counter that can be used by versions 1 and 6 UUIDs to support
/// the uniqueness of timestamps.
///
/// # References
///
/// * [UUID Version 1 in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-5.1)
/// * [UUID Version 6 in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-5.6)
/// * [UUID Generator States in RFC 9562](https://www.ietf.org/rfc/rfc9562.html#section-6.3)
pub trait ClockSequence {
    /// The type of sequence returned by this counter.
    type Output;

    /// Get the next value in the sequence to feed into a timestamp.
    ///
    /// This method will be called each time a [`Timestamp`] is constructed.
    ///
    /// Any bits beyond [`ClockSequence::usable_bits`] in the output must be unset.
    ///
    /// Note that a caller is not required to pass normalized values, so `subsec_nanos` may
    /// be greater than the actual number of nanoseconds in a second (1,000,000,000).
    /// Implementations are also not required to return normalized values, but values
    /// outside the range supported by a given UUID variant may weaken uniqueness or monotonicity.
    fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output;

    /// Get the next value in the sequence, potentially also adjusting the timestamp.
    ///
    /// This method should be preferred over `generate_sequence`.
    ///
    /// Any bits beyond [`ClockSequence::usable_bits`] in the output must be unset.
    ///
    /// Note that a caller is not required to pass normalized values, so `subsec_nanos` may
    /// be greater than the actual number of nanoseconds in a second (1,000,000,000).
    /// Implementations are also not required to return normalized values, but values
    /// outside the range supported by a given UUID variant may weaken uniqueness or monotonicity.
    ///
    /// The returned timestamp isn't required to be normalized either. Any `(seconds, subsec_nanos)`
    /// value is preserved exactly by [`Timestamp`], and [`Timestamp::to_unix`] normalizes it where possible.
    fn generate_timestamp_sequence(
        &self,
        seconds: u64,
        subsec_nanos: u32,
    ) -> (Self::Output, u64, u32) {
        (
            self.generate_sequence(seconds, subsec_nanos),
            seconds,
            subsec_nanos,
        )
    }

    /// The number of usable bits from the least significant bit in the result of [`ClockSequence::generate_sequence`]
    /// or [`ClockSequence::generate_timestamp_sequence`].
    ///
    /// The number of usable bits must not exceed 128.
    ///
    /// The number of usable bits is not expected to change between calls. An implementation of `ClockSequence` should
    /// always return the same value from this method.
    fn usable_bits(&self) -> usize
    where
        Self::Output: Sized,
    {
        cmp::min(128, core::mem::size_of::<Self::Output>() * 8)
    }
}

impl<T: ClockSequence + ?Sized> ClockSequence for &T {
    type Output = T::Output;

    fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
        (**self).generate_sequence(seconds, subsec_nanos)
    }

    fn generate_timestamp_sequence(
        &self,
        seconds: u64,
        subsec_nanos: u32,
    ) -> (Self::Output, u64, u32) {
        (**self).generate_timestamp_sequence(seconds, subsec_nanos)
    }

    fn usable_bits(&self) -> usize
    where
        Self::Output: Sized,
    {
        (**self).usable_bits()
    }
}

/// Default implementations for the [`ClockSequence`] trait.
pub mod context {
    use super::ClockSequence;

    #[cfg(any(feature = "v1", feature = "v6"))]
    mod v1_support {
        use super::*;

        #[cfg(all(feature = "std", feature = "rng"))]
        use crate::std::sync::LazyLock;

        use atomic::{Atomic, Ordering};

        #[cfg(all(feature = "std", feature = "rng"))]
        static CONTEXT: LazyLock<ContextV1> = LazyLock::new(ContextV1::new_random);

        #[cfg(all(feature = "std", feature = "rng"))]
        pub(crate) fn shared_context_v1() -> &'static ContextV1 {
            &CONTEXT
        }

        /// An internally synchronized, wrapping counter that produces 14-bit values for version 1 and version 6 UUIDs.
        ///
        /// This type is:
        ///
        /// - **Non-reseeding:** The counter is not reseeded on each time interval (100ns).
        /// - **Non-adjusting:** The timestamp is not incremented when the counter wraps within a time interval (100ns).
        /// - **Thread-safe:** The underlying counter is atomic, so can be shared across threads.
        ///
        /// This type should be used when constructing versions 1 and 6 UUIDs.
        ///
        /// This type should not be used when constructing version 7 UUIDs. When used to
        /// construct a version 7 UUID, the 14-bit counter will be padded with random data.
        /// Counter overflows are more likely with a 14-bit counter than they are with a
        /// 42-bit counter when working at millisecond precision. This type doesn't attempt
        /// to adjust the timestamp on overflow.
        #[derive(Debug)]
        pub struct ContextV1 {
            count: Atomic<u16>,
        }

        impl ContextV1 {
            /// Construct a new context that's initialized with the given value.
            ///
            /// The starting value should be a random number, so that UUIDs from
            /// different systems with the same timestamps are less likely to collide.
            /// When the `rng` feature is enabled, prefer the [`ContextV1::new_random`] method.
            pub const fn new(count: u16) -> Self {
                Self {
                    count: Atomic::<u16>::new(count),
                }
            }

            /// Construct a new context that's initialized with a random value.
            #[cfg(feature = "rng")]
            pub fn new_random() -> Self {
                Self {
                    count: Atomic::<u16>::new(crate::rng::u16()),
                }
            }
        }

        impl ClockSequence for ContextV1 {
            type Output = u16;

            fn generate_sequence(&self, _seconds: u64, _nanos: u32) -> Self::Output {
                // RFC 9562 reserves 2 bits of the clock sequence so the actual
                // maximum value is smaller than `u16::MAX`. Since we unconditionally
                // increment the clock sequence we want to wrap once it becomes larger
                // than what we can represent in a "u14". Otherwise there'd be patches
                // where the clock sequence doesn't change regardless of the timestamp
                self.count.fetch_add(1, Ordering::AcqRel) & (u16::MAX >> 2)
            }

            fn usable_bits(&self) -> usize {
                14
            }
        }

        #[deprecated(since = "1.23.0", note = "renamed to `ContextV1`")]
        #[doc(hidden)]
        pub type Context = ContextV1;

        #[cfg(test)]
        mod tests {
            use crate::Timestamp;

            use super::*;

            #[test]
            fn context() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                let context = ContextV1::new(u16::MAX >> 2);

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);
                assert_eq!(16383, ts.counter);
                assert_eq!(14, ts.usable_counter_bits);

                let seconds = 1_496_854_536;

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);
                assert_eq!(0, ts.counter);

                let seconds = 1_496_854_535;

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);
                assert_eq!(1, ts.counter);
            }

            #[test]
            fn context_overflow() {
                let seconds = u64::MAX;
                let subsec_nanos = u32::MAX;

                let context = ContextV1::new(u16::MAX);

                // Ensure we don't panic
                Timestamp::from_unix(&context, seconds, subsec_nanos);
            }
        }
    }

    #[cfg(any(feature = "v1", feature = "v6"))]
    pub use v1_support::*;

    #[cfg(feature = "std")]
    mod std_support {
        use super::*;

        use core::panic::{AssertUnwindSafe, RefUnwindSafe};
        use std::{sync::Mutex, thread::LocalKey};

        /// A wrapper for a context that uses thread-local storage.
        pub struct ThreadLocalContext<C: 'static>(&'static LocalKey<C>);

        impl<C> std::fmt::Debug for ThreadLocalContext<C> {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                f.debug_struct("ThreadLocalContext").finish_non_exhaustive()
            }
        }

        impl<C: 'static> ThreadLocalContext<C> {
            /// Wrap a thread-local container with a context.
            pub const fn new(local_key: &'static LocalKey<C>) -> Self {
                ThreadLocalContext(local_key)
            }
        }

        impl<C: ClockSequence + 'static> ClockSequence for ThreadLocalContext<C> {
            type Output = C::Output;

            fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
                self.0
                    .with(|ctxt| ctxt.generate_sequence(seconds, subsec_nanos))
            }

            fn generate_timestamp_sequence(
                &self,
                seconds: u64,
                subsec_nanos: u32,
            ) -> (Self::Output, u64, u32) {
                self.0
                    .with(|ctxt| ctxt.generate_timestamp_sequence(seconds, subsec_nanos))
            }

            fn usable_bits(&self) -> usize {
                self.0.with(|ctxt| ctxt.usable_bits())
            }
        }

        impl<C: ClockSequence> ClockSequence for AssertUnwindSafe<C> {
            type Output = C::Output;

            fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
                self.0.generate_sequence(seconds, subsec_nanos)
            }

            fn generate_timestamp_sequence(
                &self,
                seconds: u64,
                subsec_nanos: u32,
            ) -> (Self::Output, u64, u32) {
                self.0.generate_timestamp_sequence(seconds, subsec_nanos)
            }

            fn usable_bits(&self) -> usize
            where
                Self::Output: Sized,
            {
                self.0.usable_bits()
            }
        }

        impl<C: ClockSequence + RefUnwindSafe> ClockSequence for Mutex<C> {
            type Output = C::Output;

            fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
                self.lock()
                    .unwrap_or_else(|err| err.into_inner())
                    .generate_sequence(seconds, subsec_nanos)
            }

            fn generate_timestamp_sequence(
                &self,
                seconds: u64,
                subsec_nanos: u32,
            ) -> (Self::Output, u64, u32) {
                self.lock()
                    .unwrap_or_else(|err| err.into_inner())
                    .generate_timestamp_sequence(seconds, subsec_nanos)
            }

            fn usable_bits(&self) -> usize
            where
                Self::Output: Sized,
            {
                self.lock()
                    .unwrap_or_else(|err| err.into_inner())
                    .usable_bits()
            }
        }
    }

    #[cfg(feature = "std")]
    pub use std_support::*;

    #[cfg(feature = "v7")]
    mod v7_support {
        use super::*;

        use crate::timestamp::parts_to_nanos;
        use core::{cell::Cell, cmp, panic::RefUnwindSafe};

        #[cfg(feature = "std")]
        static CONTEXT_V7: SharedContextV7 =
            SharedContextV7(std::sync::Mutex::new(ContextV7::new()));

        #[cfg(feature = "std")]
        pub(crate) fn shared_context_v7() -> &'static SharedContextV7 {
            &CONTEXT_V7
        }

        const USABLE_BITS: usize = 42;

        // Leave the most significant bit unset
        // This guarantees the counter has at least 2,199,023,255,552
        // values before it will overflow, which is exceptionally unlikely
        // even in the worst case
        const RESEED_MASK: u64 = u64::MAX >> 23;
        const MAX_COUNTER: u64 = u64::MAX >> 22;

        /// An unsynchronized, reseeding counter that produces 42-bit values for version 7 UUIDs.
        ///
        /// This type is:
        ///
        /// - **Reseeding:** The counter is reseeded on each time interval (1ms) with a random 41-bit value.
        ///   The 42nd bit is left unset so the counter can safely increment over the millisecond.
        /// - **Adjusting:** The timestamp is incremented when the counter wraps within a time interval (1ms).
        ///   All subsequent timestamps in that same interval will also be incremented until it changes and
        ///   the counter is reseeded.
        /// - **Non-thread-safe:** The underlying counter uses unsynchronized cells, so needs to be wrapped in
        ///   a mutex to share.
        ///
        /// The counter can use additional sub-millisecond precision from the timestamp to better
        /// synchronize UUID sorting in distributed systems. In these cases, the additional precision
        /// is masked into the left-most 12 bits of the counter. The counter is still reseeded on
        /// each new millisecond, and incremented within the millisecond. This behavior may change
        /// in the future. The only guarantee is monotonicity.
        ///
        /// This type can be used when constructing version 7 UUIDs. When used to construct a
        /// version 7 UUID, the 42-bit counter will be padded with random data. This type can
        /// be used to maintain ordering of UUIDs within the same millisecond.
        ///
        /// This type should not be used when constructing version 1 or version 6 UUIDs.
        /// When used to construct a version 1 or version 6 UUID, only the 14 least significant
        /// bits of the counter will be used.
        #[derive(Debug, Default)]
        pub struct ContextV7 {
            timestamp: Cell<ReseedingTimestamp>,
            counter: Cell<Counter>,
            adjust: Adjust,
            precision: Precision,
        }

        impl RefUnwindSafe for ContextV7 {}

        impl ContextV7 {
            /// Construct a new context that will reseed its counter on the first
            /// non-zero timestamp it receives.
            pub const fn new() -> Self {
                ContextV7 {
                    timestamp: Cell::new(ReseedingTimestamp {
                        last_seed: 0,
                        unix_nanos: 0,
                    }),
                    counter: Cell::new(Counter { value: 0 }),
                    adjust: Adjust { by_ns: 0 },
                    precision: Precision {
                        bits: 0,
                        mask: 0,
                        factor: 0,
                        shift: 0,
                    },
                }
            }

            /// Specify an amount to shift timestamps by to obfuscate their actual generation time.
            pub fn with_adjust_by_millis(mut self, millis: u32) -> Self {
                self.adjust = Adjust::by_millis(millis);
                self
            }

            /// Use the leftmost 12 bits of the counter for additional timestamp precision.
            ///
            /// This method can provide better sorting for distributed applications that generate frequent UUIDs
            /// by trading a small amount of entropy for better counter synchronization. Note that the counter
            /// will still be reseeded on millisecond boundaries, even though some of its storage will be
            /// dedicated to the timestamp.
            pub fn with_additional_precision(self) -> Self {
                self.with_additional_precision_bits(12)
            }

            /// Use the leftmost `bits` bits of the counter for additional timestamp precision.
            ///
            /// This is a more general form of [`ContextV7::with_additional_precision`] for platforms
            /// whose clocks don't have the resolution to make use of the full 12 bits. A platform with
            /// microsecond precision can set 10 bits here, leaving the remaining 2 bits of the `rand_a`
            /// field free for random data.
            ///
            /// `bits` is capped at 12, matching the size of the `rand_a` field in a version 7 UUID.
            /// Passing 0 disables additional precision, the same as never calling this method.
            pub fn with_additional_precision_bits(mut self, bits: usize) -> Self {
                self.precision = Precision::new(cmp::min(bits, 12));
                self
            }
        }

        impl ClockSequence for ContextV7 {
            type Output = u64;

            fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
                self.generate_timestamp_sequence(seconds, subsec_nanos).0
            }

            fn generate_timestamp_sequence(
                &self,
                seconds: u64,
                subsec_nanos: u32,
            ) -> (Self::Output, u64, u32) {
                let unix_nanos = self.adjust.apply(parts_to_nanos(seconds, subsec_nanos));

                let mut counter;
                let (mut timestamp, should_reseed) = self.timestamp.get().advance(unix_nanos);

                if should_reseed {
                    // If the observed system time has shifted forwards then regenerate the counter
                    counter = Counter::reseed(&self.precision, &timestamp);
                } else {
                    // If the observed system time has not shifted forwards then increment the counter

                    // If the incoming timestamp is earlier than the last observed one then
                    // use it instead. This may happen if the system clock jitters, or if the counter
                    // has wrapped and the timestamp is artificially incremented

                    counter = self.counter.get().increment(&self.precision, &timestamp);

                    // Unlikely: If the counter has overflowed its 42-bit storage then wrap it
                    // and increment the timestamp. Until the observed system time shifts past
                    // this incremented value, all timestamps will use it to maintain monotonicity
                    if counter.has_overflowed() {
                        // Increment the timestamp by 1 milli and reseed the counter
                        timestamp = timestamp.increment();
                        counter = Counter::reseed(&self.precision, &timestamp);
                    }
                };

                self.timestamp.set(timestamp);
                self.counter.set(counter);

                let (seconds, subsec_nanos) = timestamp.to_parts();

                (counter.value, seconds, subsec_nanos)
            }

            fn usable_bits(&self) -> usize {
                USABLE_BITS
            }
        }

        /// A timestamp that keeps track of whether a reseed is necessary.
        #[derive(Debug, Default, Clone, Copy)]
        struct ReseedingTimestamp {
            last_seed: u64,
            unix_nanos: u128,
        }

        impl ReseedingTimestamp {
            #[inline]
            fn saturating_from_unix_nanos(unix_nanos: u128) -> Self {
                const MAX_UNIX_NANOS: u128 = (u64::MAX as u128) * 1_000_000 + 999_999;

                let unix_nanos = cmp::min(unix_nanos, MAX_UNIX_NANOS);

                // Reseed when the millisecond advances
                let last_seed = (unix_nanos / 1_000_000) as u64;

                ReseedingTimestamp {
                    last_seed,
                    unix_nanos,
                }
            }

            #[inline]
            fn to_parts(&self) -> (u64, u32) {
                // Guaranteed never to overflow `(u64::MAX, 999_999_999)`
                (
                    (self.unix_nanos / 1_000_000_000) as u64,
                    (self.unix_nanos % 1_000_000_000) as u32,
                )
            }

            /// Advance the timestamp to a new value, returning whether a reseed is necessary.
            #[inline]
            fn advance(&self, unix_nanos: u128) -> (Self, bool) {
                let incoming = ReseedingTimestamp::saturating_from_unix_nanos(unix_nanos);

                (
                    ReseedingTimestamp::saturating_from_unix_nanos(cmp::max(
                        self.unix_nanos,
                        unix_nanos,
                    )),
                    incoming.last_seed > self.last_seed,
                )
            }

            /// Advance the timestamp by a millisecond.
            #[inline]
            fn increment(&self) -> Self {
                ReseedingTimestamp::saturating_from_unix_nanos(
                    Adjust::by_millis(1).apply(self.unix_nanos),
                )
            }
        }

        /// A counter that initializes to a safe random seed and tracks overflow.
        #[derive(Debug, Default, Clone, Copy)]
        struct Counter {
            value: u64,
        }

        impl Counter {
            #[inline]
            fn reseed(precision: &Precision, timestamp: &ReseedingTimestamp) -> Self {
                Counter {
                    value: precision.apply(crate::rng::u64() & RESEED_MASK, timestamp),
                }
            }

            /// Advance the counter.
            ///
            /// This method is guaranteed to return a counter with a higher value than the current one.
            /// Additional precision from the timestamp will be used, unless it cause the counter
            /// to shift backwards.
            #[inline]
            fn increment(&self, precision: &Precision, timestamp: &ReseedingTimestamp) -> Self {
                // We unconditionally increment the counter even though the precision
                // may have set higher bits anyway. This could technically be avoided,
                // but the higher bits are a coarse approximation so we just avoid the
                // `if` branch and increment it either way

                // Guaranteed to never overflow u64
                let next = self.value + 1;

                Counter {
                    value: cmp::max(next, precision.apply(next, timestamp)),
                }
            }

            #[inline]
            fn has_overflowed(&self) -> bool {
                self.value > MAX_COUNTER
            }
        }

        /// A utility that adjusts an input timestamp by a given number of nanoseconds.
        #[derive(Debug, Default)]
        struct Adjust {
            by_ns: u128,
        }

        impl Adjust {
            #[inline]
            fn by_millis(millis: u32) -> Self {
                Adjust {
                    by_ns: (millis as u128).saturating_mul(1_000_000),
                }
            }

            /// Apply the adjustment, returning the adjusted timestamp.
            #[inline]
            fn apply(&self, nanos: u128) -> u128 {
                nanos.saturating_add(self.by_ns)
            }
        }

        /// A utility that overwrites some number of counter bits with additional timestamp precision.
        #[derive(Debug, Default)]
        struct Precision {
            bits: usize,
            factor: u64,
            mask: u64,
            shift: u64,
        }

        impl Precision {
            fn new(bits: usize) -> Self {
                // The mask and shift are used to paste the sub-millisecond precision
                // into the most significant bits of the counter
                let mask = u64::MAX >> (64 - USABLE_BITS + bits);
                let shift = (USABLE_BITS - bits) as u64;

                // The factor reduces the size of the sub-millisecond precision to
                // fit into the specified number of bits
                let factor = (999_999 / u64::pow(2, bits as u32)) + 1;

                Precision {
                    bits,
                    factor,
                    mask,
                    shift,
                }
            }

            /// Apply additional precision from the given timestamp to the counter.
            #[inline]
            fn apply(&self, counter: u64, timestamp: &ReseedingTimestamp) -> u64 {
                if self.bits == 0 {
                    // No additional precision is being used
                    return counter;
                }

                let additional = (timestamp.unix_nanos % 1_000_000) as u64 / self.factor;

                (counter & self.mask) | (additional << self.shift)
            }
        }

        #[cfg(feature = "std")]
        pub(crate) struct SharedContextV7(std::sync::Mutex<ContextV7>);

        #[cfg(feature = "std")]
        impl ClockSequence for SharedContextV7 {
            type Output = u64;

            fn generate_sequence(&self, seconds: u64, subsec_nanos: u32) -> Self::Output {
                self.0.generate_sequence(seconds, subsec_nanos)
            }

            fn generate_timestamp_sequence(
                &self,
                seconds: u64,
                subsec_nanos: u32,
            ) -> (Self::Output, u64, u32) {
                self.0.generate_timestamp_sequence(seconds, subsec_nanos)
            }

            fn usable_bits(&self) -> usize
            where
                Self::Output: Sized,
            {
                USABLE_BITS
            }
        }

        #[cfg(test)]
        mod tests {
            use core::time::Duration;
            use std::boxed::Box;

            use super::*;

            use crate::{Timestamp, Uuid};

            fn builder(f: impl Fn() -> ContextV7 + 'static) -> Box<dyn Fn() -> ContextV7> {
                Box::new(f)
            }

            #[test]
            fn context() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                let context = ContextV7::new();

                let ts1 = Timestamp::from_unix(&context, seconds, subsec_nanos);
                assert_eq!(42, ts1.usable_counter_bits);

                // Backwards second
                let seconds = 1_496_854_534;

                let ts2 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // The backwards time should be ignored
                // The counter should still increment
                assert_eq!(ts1.unix_nanos, ts2.unix_nanos);
                assert_eq!(ts1.counter + 1, ts2.counter);

                // Forwards second
                let seconds = 1_496_854_536;

                let ts3 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // The counter should have reseeded
                assert_ne!(ts2.counter + 1, ts3.counter);
                assert_ne!(0, ts3.counter);
            }

            #[test]
            fn context_wrap() {
                let seconds = 1_496_854_535u64;
                let subsec_nanos = 812_946_000u32;

                // This context will wrap
                let context = ContextV7 {
                    timestamp: Cell::new(ReseedingTimestamp::saturating_from_unix_nanos(
                        parts_to_nanos(seconds, subsec_nanos),
                    )),
                    adjust: Adjust::by_millis(0),
                    precision: Precision {
                        bits: 0,
                        mask: 0,
                        factor: 0,
                        shift: 0,
                    },
                    counter: Cell::new(Counter {
                        value: u64::MAX >> 22,
                    }),
                };

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // The timestamp should be incremented by 1ms
                let expected_ts = Duration::new(seconds, subsec_nanos) + Duration::from_millis(1);
                assert_eq!(expected_ts.as_secs(), ts.to_unix().0);
                assert_eq!(expected_ts.subsec_nanos(), ts.to_unix().1);

                // The counter should have reseeded
                assert!(ts.counter < (u64::MAX >> 22) as u128);
                assert_ne!(0, ts.counter);
            }

            #[test]
            fn context_shift() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                let context = ContextV7::new().with_adjust_by_millis(1);

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);

                assert_eq!((1_496_854_535, 813_946_000), ts.to_unix());
            }

            #[test]
            fn context_additional_precision() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                let context = ContextV7::new().with_additional_precision();

                let ts1 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // NOTE: Future changes in rounding may change this value slightly
                assert_eq!(3861, ts1.counter >> 30);

                assert!(ts1.counter < (u64::MAX >> 22) as u128);

                // Generate another timestamp; it should continue to sort
                let ts2 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                assert!(Uuid::new_v7(ts2) > Uuid::new_v7(ts1));

                // Generate another timestamp with an extra nanosecond
                let subsec_nanos = subsec_nanos + 1;

                let ts3 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                assert!(Uuid::new_v7(ts3) > Uuid::new_v7(ts2));
            }

            #[test]
            fn context_additional_precision_bits() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                // 10 bits leaves the low 2 bits of `rand_a` for random data, which suits
                // platforms with microsecond precision
                let context = ContextV7::new().with_additional_precision_bits(10);

                let ts = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // The submillisecond precision occupies the leftmost 10 of the 42 counter bits
                // NOTE: Future changes in rounding may change this value slightly
                assert_eq!(968, ts.counter >> 32);

                assert!(ts.counter < (u64::MAX >> 22) as u128);

                // The full method is just the 12-bit form of this one
                let full = Timestamp::from_unix(
                    &ContextV7::new().with_additional_precision(),
                    seconds,
                    subsec_nanos,
                );
                let bits12 = Timestamp::from_unix(
                    &ContextV7::new().with_additional_precision_bits(12),
                    seconds,
                    subsec_nanos,
                );
                assert_eq!(full.counter >> 30, bits12.counter >> 30);

                // Values above 12 are capped at 12
                let capped = Timestamp::from_unix(
                    &ContextV7::new().with_additional_precision_bits(64),
                    seconds,
                    subsec_nanos,
                );
                assert_eq!(bits12.counter >> 30, capped.counter >> 30);

                // Zero bits disables additional precision entirely
                let none = ContextV7::new().with_additional_precision_bits(0);
                assert_eq!(0, none.precision.bits);
            }

            #[test]
            fn context_additional_precision_carry() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_946_000;

                for bits in 1..=12 {
                    let precision = Precision::new(bits);
                    let timestamp = ReseedingTimestamp::saturating_from_unix_nanos(parts_to_nanos(
                        seconds,
                        subsec_nanos,
                    ));

                    // This context will carry into the additional precision
                    let context = ContextV7 {
                        counter: Cell::new(Counter {
                            value: precision.apply(u64::MAX, &timestamp),
                        }),
                        timestamp: Cell::new(timestamp),
                        adjust: Adjust::by_millis(0),
                        precision,
                    };

                    let a = Timestamp::from_unix(&context, seconds, subsec_nanos);
                    let b = Timestamp::from_unix(&context, seconds, subsec_nanos);
                    let c = Timestamp::from_unix(&context, seconds, subsec_nanos);

                    assert!(Uuid::new_v7(a) < Uuid::new_v7(b), "a: {a:?} < b: {b:?}");
                    assert!(Uuid::new_v7(b) < Uuid::new_v7(c), "b: {b:?} < c: {c:?}");
                }
            }

            #[test]
            fn context_overflow() {
                let seconds = u64::MAX;
                let subsec_nanos = u32::MAX;

                // Ensure we don't panic
                for context in [
                    builder(|| ContextV7::new()),
                    builder(|| ContextV7::new().with_additional_precision()),
                    builder(|| ContextV7::new().with_adjust_by_millis(u32::MAX)),
                    builder(|| {
                        ContextV7::new()
                            .with_additional_precision()
                            .with_adjust_by_millis(u32::MAX)
                    }),
                ] {
                    Timestamp::from_unix(&context(), seconds, subsec_nanos);
                }
            }

            #[test]
            fn context_subsec_ordering() {
                for context in [
                    builder(|| ContextV7::new()),
                    builder(|| ContextV7::new().with_additional_precision()),
                    builder(|| ContextV7::new().with_adjust_by_millis(u32::MAX)),
                    builder(|| {
                        ContextV7::new()
                            .with_additional_precision()
                            .with_adjust_by_millis(u32::MAX)
                    }),
                ] {
                    for (a, b, c) in [
                        // Wall-clock order of incoming timestamps is b, a, c
                        ((100, 0), (99, 2_000_000), (100, 1_000_000)),
                        ((100, 0), (99, 1_000_900_000), (100, 1_000_000)),
                    ] {
                        let context = context();

                        // UUID order is a, b, c
                        let a = Timestamp::from_unix(&context, a.0, a.1);
                        let b = Timestamp::from_unix(&context, b.0, b.1);
                        let c = Timestamp::from_unix(&context, c.0, c.1);

                        assert!(Uuid::new_v7(a) < Uuid::new_v7(b), "a: {a:?} < b: {b:?}");
                        assert!(Uuid::new_v7(b) < Uuid::new_v7(c), "b: {b:?} < c: {c:?}");
                    }
                }
            }

            #[test]
            fn context_subsec_saturated_ordering() {
                for context in [
                    builder(|| ContextV7::new()),
                    builder(|| ContextV7::new().with_additional_precision()),
                    builder(|| ContextV7::new().with_adjust_by_millis(u32::MAX)),
                    builder(|| {
                        ContextV7::new()
                            .with_additional_precision()
                            .with_adjust_by_millis(u32::MAX)
                    }),
                ] {
                    let context = context();

                    let secs = (u64::MAX / 1_000) + 1;

                    let a = Timestamp::from_unix(&context, secs, 900_000);
                    let b = Timestamp::from_unix(&context, secs, 1_000_000);

                    assert!(Uuid::new_v7(a) < Uuid::new_v7(b), "a: {a:?} < b: {b:?}");
                }
            }

            #[test]
            fn context_additional_precision_within_millisecond() {
                let seconds = 1_496_854_535;
                let subsec_nanos = 812_100_000;

                let context = ContextV7::new().with_additional_precision();

                let ts1 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                // Later in the same millisecond
                let ts2 = Timestamp::from_unix(&context, seconds, subsec_nanos + 500_000);

                assert!(
                    ts2.counter >> 30 > ts1.counter >> 30,
                    "ts1: {ts1:?}, ts2: {ts2:?}"
                );

                // Earlier in the same millisecond
                let ts3 = Timestamp::from_unix(&context, seconds, subsec_nanos);

                assert_eq!(
                    ts2.counter >> 30,
                    ts3.counter >> 30,
                    "ts2: {ts2:?}, ts3: {ts3:?}"
                );
                assert_eq!(ts2.to_unix(), ts3.to_unix());

                assert!(Uuid::new_v7(ts1) < Uuid::new_v7(ts2));
                assert!(Uuid::new_v7(ts2) < Uuid::new_v7(ts3));
            }

            #[test]
            fn reseeding_timestamp_saturating() {
                const MAX_UNIX_NANOS: u128 = (u64::MAX as u128) * 1_000_000 + 999_999;
                const MAX_PARTS: (u64, u32) = (u64::MAX / 1_000, 615_999_999);

                // Values at or below the ceiling pass through
                let below =
                    ReseedingTimestamp::saturating_from_unix_nanos(MAX_UNIX_NANOS - 1_000_000);
                assert_eq!(MAX_UNIX_NANOS - 1_000_000, below.unix_nanos);
                assert_eq!(u64::MAX - 1, below.last_seed);
                assert_eq!((u64::MAX / 1_000, 614_999_999), below.to_parts());

                // Values beyond the ceiling are clamped to it
                for unix_nanos in [
                    MAX_UNIX_NANOS,
                    MAX_UNIX_NANOS + 1,
                    parts_to_nanos(u64::MAX / 1_000 + 1, 0),
                    parts_to_nanos(u64::MAX, 999_999_999),
                    parts_to_nanos(u64::MAX, u32::MAX),
                    u128::MAX,
                ] {
                    let ts = ReseedingTimestamp::saturating_from_unix_nanos(unix_nanos);

                    assert_eq!(MAX_UNIX_NANOS, ts.unix_nanos, "{unix_nanos}");
                    assert_eq!(u64::MAX, ts.last_seed, "{unix_nanos}");
                    assert_eq!(MAX_PARTS, ts.to_parts(), "{unix_nanos}");
                }

                // Incrementing at the ceiling stays there instead of wrapping
                let max = ReseedingTimestamp::saturating_from_unix_nanos(MAX_UNIX_NANOS);
                let incremented = max.increment();
                assert_eq!(MAX_UNIX_NANOS, incremented.unix_nanos);
                assert_eq!(u64::MAX, incremented.last_seed);
            }

            #[test]
            fn context_saturated_parts() {
                const MAX_PARTS: (u64, u32) = (u64::MAX / 1_000, 615_999_999);

                for context in [
                    builder(|| ContextV7::new()),
                    builder(|| ContextV7::new().with_adjust_by_millis(u32::MAX)),
                ] {
                    for (seconds, subsec_nanos) in [
                        MAX_PARTS,
                        (u64::MAX / 1_000 + 1, 0),
                        (u64::MAX, 999_999_999),
                        (u64::MAX, u32::MAX),
                    ] {
                        let ts = Timestamp::from_unix(&context(), seconds, subsec_nanos);

                        assert_eq!(MAX_PARTS, ts.to_unix(), "{seconds}, {subsec_nanos}");
                    }
                }

                // A counter overflow at the ceiling can't advance the timestamp, but it mustn't wrap it either
                let context = ContextV7 {
                    timestamp: Cell::new(ReseedingTimestamp::saturating_from_unix_nanos(u128::MAX)),
                    adjust: Adjust::by_millis(0),
                    precision: Precision::new(0),
                    counter: Cell::new(Counter {
                        value: u64::MAX >> 22,
                    }),
                };

                let ts = Timestamp::from_unix(&context, u64::MAX, u32::MAX);

                assert_eq!(MAX_PARTS, ts.to_unix());
            }
        }
    }

    #[cfg(feature = "v7")]
    pub use v7_support::*;

    /// An empty counter that will always return the value `0`.
    ///
    /// This type can be used when constructing version 7 UUIDs. When used to
    /// construct a version 7 UUID, the entire counter segment of the UUID will be
    /// filled with a random value. This type does not maintain ordering of UUIDs
    /// within a millisecond but is efficient.
    ///
    /// This type should not be used when constructing version 1 or version 6 UUIDs.
    /// When used to construct a version 1 or version 6 UUID, the counter
    /// segment will remain zero.
    #[derive(Debug, Clone, Copy, Default)]
    pub struct NoContext;

    impl ClockSequence for NoContext {
        type Output = u16;

        fn generate_sequence(&self, _seconds: u64, _nanos: u32) -> Self::Output {
            0
        }

        fn usable_bits(&self) -> usize {
            0
        }
    }
}

#[cfg(all(test, any(feature = "v1", feature = "v6")))]
mod tests {
    use super::*;

    #[cfg(all(target_family = "wasm", any(target_os = "unknown", target_os = "none")))]
    use wasm_bindgen_test::*;

    #[test]
    #[cfg_attr(
        all(target_family = "wasm", any(target_os = "unknown", target_os = "none")),
        wasm_bindgen_test
    )]
    fn gregorian_unix_does_not_panic() {
        // Ensure timestamp conversions never panic
        Timestamp::unix_to_gregorian_ticks(u64::MAX, 0);
        Timestamp::unix_to_gregorian_ticks(0, u32::MAX);
        Timestamp::unix_to_gregorian_ticks(u64::MAX, u32::MAX);

        Timestamp::gregorian_to_unix(u64::MAX);
    }

    #[test]
    #[cfg_attr(
        all(target_family = "wasm", any(target_os = "unknown", target_os = "none")),
        wasm_bindgen_test
    )]
    fn to_gregorian_truncates_to_usable_bits() {
        let ts = Timestamp::from_gregorian_time(123, u16::MAX);

        assert_eq!((123, u16::MAX >> 2), ts.to_gregorian());
    }

    #[test]
    #[cfg_attr(
        all(target_family = "wasm", any(target_os = "unknown", target_os = "none")),
        wasm_bindgen_test
    )]
    fn clock_sequence_usable_bits() {
        struct MyContext;

        impl ClockSequence for MyContext {
            type Output = u16;

            fn generate_sequence(&self, _: u64, _: u32) -> Self::Output {
                0
            }
        }

        assert_eq!(16, MyContext.usable_bits());
    }

    const MAX_PARTS_NANOS: u128 = (u64::MAX as u128) * 1_000_000_000 + 999_999_999;

    #[test]
    fn nanos_to_parts_roundtrip() {
        for case in [
            0,
            1,
            999_999_999,
            1_000_000_000,
            u64::MAX as u128,
            u64::MAX as u128 + 1,
            u64::MAX as u128 + u32::MAX as u128,
            u64::MAX as u128 + u32::MAX as u128 + 1,
            (u64::MAX as u128) * 1_000_000 + 999_999,
            (u64::MAX as u128) * 1_000_000_000,
            MAX_PARTS_NANOS - 1,
            MAX_PARTS_NANOS,
        ] {
            let (seconds, subsec_nanos) = nanos_to_parts(case);

            assert!(subsec_nanos < 1_000_000_000, "{case}");
            assert_eq!(case, parts_to_nanos(seconds, subsec_nanos), "{case}");
        }
    }

    #[test]
    fn nanos_to_parts_unnormalized_roundtrip() {
        // Beyond the largest normalized value, seconds can't take the carry,
        // so the remainder stays in the subsecond nanos
        for (case, expected) in [
            (MAX_PARTS_NANOS + 1, (u64::MAX, 1_000_000_000)),
            (MAX_PARTS_NANOS + 2, (u64::MAX, 1_000_000_001)),
            (
                parts_to_nanos(u64::MAX, u32::MAX) - 1,
                (u64::MAX, u32::MAX - 1),
            ),
            (parts_to_nanos(u64::MAX, u32::MAX), (u64::MAX, u32::MAX)),
        ] {
            let (seconds, subsec_nanos) = nanos_to_parts(case);

            assert_eq!(expected, (seconds, subsec_nanos), "{case}");
            assert_eq!(case, parts_to_nanos(seconds, subsec_nanos), "{case}");
        }
    }

    #[test]
    fn nanos_to_parts_saturating() {
        for case in [parts_to_nanos(u64::MAX, u32::MAX) + 1, u128::MAX] {
            let (seconds, subsec_nanos) = nanos_to_parts(case);

            assert_eq!(u64::MAX, seconds, "{case}");
            assert_eq!(u32::MAX, subsec_nanos, "{case}");
        }
    }

    #[test]
    fn parts_to_nanos_unnormalized() {
        assert_eq!(2_500_000_000, parts_to_nanos(1, 1_500_000_000));
        assert_eq!(
            parts_to_nanos(2, 500_000_000),
            parts_to_nanos(1, 1_500_000_000)
        );

        // The largest input doesn't overflow
        assert_eq!(
            (u64::MAX as u128) * 1_000_000_000 + u32::MAX as u128,
            parts_to_nanos(u64::MAX, u32::MAX)
        );
    }

    #[test]
    fn timestamp_unix_parts_boundaries() {
        // Normalized values round-trip
        for (seconds, subsec_nanos) in [
            (0, 0),
            (1, 999_999_999),
            (u64::MAX, 0),
            (u64::MAX, 999_999_999),
        ] {
            let ts = Timestamp::from_unix_time(seconds, subsec_nanos, 0, 0);

            assert_eq!((seconds, subsec_nanos), ts.to_unix());
        }

        // Unnormalized values are normalized
        let ts = Timestamp::from_unix_time(1, 1_500_000_000, 0, 0);
        assert_eq!((2, 500_000_000), ts.to_unix());

        // Values beyond the largest normalized are preserved
        for (seconds, subsec_nanos) in [(u64::MAX, 1_000_000_000), (u64::MAX, u32::MAX)] {
            let ts = Timestamp::from_unix_time(seconds, subsec_nanos, 0, 0);

            assert_eq!((seconds, subsec_nanos), ts.to_unix());
        }

        let ts = Timestamp::from_unix_time(u64::MAX - 1, u32::MAX, 0, 0);
        assert_eq!((u64::MAX, u32::MAX - 1_000_000_000), ts.to_unix());
    }

    #[cfg(all(test, feature = "std", not(miri)))]
    mod std_support {
        use super::*;

        use std::time::{Duration, SystemTime};

        // Components of an arbitrary timestamp with non-zero nanoseconds.
        const KNOWN_SECONDS: u64 = 1_501_520_400;
        const KNOWN_NANOS: u32 = 1_000;

        fn known_system_time() -> SystemTime {
            SystemTime::UNIX_EPOCH
                .checked_add(Duration::new(KNOWN_SECONDS, KNOWN_NANOS))
                .unwrap()
        }

        fn known_timestamp() -> Timestamp {
            Timestamp::from_unix_time(KNOWN_SECONDS, KNOWN_NANOS, 0, 0)
        }

        #[test]
        fn to_system_time() {
            let st: SystemTime = known_timestamp().into();

            assert_eq!(known_system_time(), st);
        }

        #[test]
        fn from_system_time() {
            let ts: Timestamp = known_system_time().try_into().unwrap();

            assert_eq!(known_timestamp(), ts);
        }

        #[test]
        fn from_system_time_before_epoch() {
            let before_epoch = match SystemTime::UNIX_EPOCH.checked_sub(Duration::from_nanos(1_000))
            {
                Some(st) => st,
                None => return,
            };

            Timestamp::try_from(before_epoch)
                .expect_err("Timestamp should not be created from before epoch");
        }

        #[test]
        fn from_system_time_max() {
            for (seconds, subsec_nanos) in [
                (u64::MAX, 999_999_999),
                (u64::MAX, 1_000_000_000),
                (u64::MAX, u32::MAX),
            ] {
                let ts = Timestamp::from_unix_time(seconds, subsec_nanos, 0, 0);

                // Just make sure we don't panic
                let _: SystemTime = ts.into();
            }
        }
    }
}
