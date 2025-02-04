// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::fallback::LocaleFallbackConfig;
use crate::{DataError, DataErrorKind, DataLocale, DataProvider, DataProviderWithMarker};
use core::fmt;
use core::marker::PhantomData;
use yoke::Yokeable;
use zerovec::ule::*;

/// Trait marker for data structs. All types delivered by the data provider must be associated with
/// something implementing this trait.
///
/// Structs implementing this trait are normally generated with the [`data_struct`] macro.
///
/// By convention, the non-standard `Marker` suffix is used by types implementing DynamicDataMarker.
///
/// In addition to a marker type implementing DynamicDataMarker, the following impls must also be present
/// for the data struct:
///
/// - `impl<'a> Yokeable<'a>` (required)
/// - `impl ZeroFrom<Self>`
///
/// Also see [`DataMarker`].
///
/// Note: `DynamicDataMarker`s are quasi-const-generic compile-time objects, and as such are expected
/// to be unit structs. As this is not something that can be enforced by the type system, we
/// currently only have a `'static` bound on them (which is needed by a lot of our code).
///
/// # Examples
///
/// Manually implementing DynamicDataMarker for a custom type:
///
/// ```
/// use icu_provider::prelude::*;
/// use std::borrow::Cow;
///
/// #[derive(yoke::Yokeable, zerofrom::ZeroFrom)]
/// struct MyDataStruct<'data> {
///     message: Cow<'data, str>,
/// }
///
/// struct MyDataStructMarker;
///
/// impl DynamicDataMarker for MyDataStructMarker {
///     type DataStruct = MyDataStruct<'static>;
/// }
///
/// // We can now use MyDataStruct with DataProvider:
/// let s = MyDataStruct {
///     message: Cow::Owned("Hello World".into()),
/// };
/// let payload = DataPayload::<MyDataStructMarker>::from_owned(s);
/// assert_eq!(payload.get().message, "Hello World");
/// ```
///
/// [`data_struct`]: crate::data_struct
pub trait DynamicDataMarker: 'static {
    /// A type that implements [`Yokeable`]. This should typically be the `'static` version of a
    /// data struct.
    type DataStruct: for<'a> Yokeable<'a>;
}

/// A [`DynamicDataMarker`] with a [`DataMarkerInfo`] attached.
///
/// Structs implementing this trait are normally generated with the [`data_struct!`] macro.
///
/// Implementing this trait enables this marker to be used with the main [`DataProvider`] trait.
/// Most markers should be associated with a specific marker and should therefore implement this
/// trait.
///
/// [`BufferMarker`] and [`AnyMarker`] are examples of markers that do _not_ implement this trait
/// because they are not specific to a single marker.
///
/// Note: `DataMarker`s are quasi-const-generic compile-time objects, and as such are expected
/// to be unit structs. As this is not something that can be enforced by the type system, we
/// currently only have a `'static` bound on them (which is needed by a lot of our code).
///
/// [`data_struct!`]: crate::data_struct
/// [`DataProvider`]: crate::DataProvider
/// [`BufferMarker`]: crate::buf::BufferMarker
/// [`AnyMarker`]: crate::any::AnyMarker
pub trait DataMarker: DynamicDataMarker {
    /// The single [`DataMarkerInfo`] associated with this marker.
    const INFO: DataMarkerInfo;

    /// Binds this [`DataMarker`] to a provider supporting it.
    fn bind<P>(provider: P) -> DataProviderWithMarker<Self, P>
    where
        P: DataProvider<Self>,
        Self: Sized,
    {
        DataProviderWithMarker::new(provider)
    }
}

/// A [`DynamicDataMarker`] that never returns data.
///
/// All types that have non-blanket impls of `DataProvider<M>` are expected to explicitly
/// implement `DataProvider<NeverMarker<Y>>`, returning [`DataErrorKind::MarkerNotFound`].
/// See [`impl_data_provider_never_marker!`].
///
/// [`DataErrorKind::MarkerNotFound`]: crate::DataErrorKind::MarkerNotFound
/// [`impl_data_provider_never_marker!`]: crate::marker::impl_data_provider_never_marker
///
/// # Examples
///
/// ```
/// use icu_locale_core::langid;
/// use icu_provider::hello_world::*;
/// use icu_provider::marker::NeverMarker;
/// use icu_provider::prelude::*;
///
/// let buffer_provider = HelloWorldProvider.into_json_provider();
///
/// let result = DataProvider::<NeverMarker<HelloWorldV1<'static>>>::load(
///     &buffer_provider.as_deserializing(),
///     DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("en").into()),
///         ..Default::default()
///     },
/// );
///
/// assert!(matches!(
///     result,
///     Err(DataError {
///         kind: DataErrorKind::MarkerNotFound,
///         ..
///     })
/// ));
/// ```
#[derive(Debug, Copy, Clone)]
pub struct NeverMarker<Y>(PhantomData<Y>);

impl<Y> DynamicDataMarker for NeverMarker<Y>
where
    for<'a> Y: Yokeable<'a>,
{
    type DataStruct = Y;
}

impl<Y> DataMarker for NeverMarker<Y>
where
    for<'a> Y: Yokeable<'a>,
{
    const INFO: DataMarkerInfo = DataMarkerInfo::from_path(data_marker_path!("_never@1"));
}

/// Implements `DataProvider<NeverMarker<Y>>` on a struct.
///
/// For more information, see [`NeverMarker`].
///
/// # Examples
///
/// ```
/// use icu_locale_core::langid;
/// use icu_provider::hello_world::*;
/// use icu_provider::marker::NeverMarker;
/// use icu_provider::prelude::*;
///
/// struct MyProvider;
///
/// icu_provider::marker::impl_data_provider_never_marker!(MyProvider);
///
/// let result = DataProvider::<NeverMarker<HelloWorldV1<'static>>>::load(
///     &MyProvider,
///     DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("und").into()),
///         ..Default::default()
///     },
/// );
///
/// assert!(matches!(
///     result,
///     Err(DataError {
///         kind: DataErrorKind::MarkerNotFound,
///         ..
///     })
/// ));
/// ```
#[doc(hidden)] // macro
#[macro_export]
macro_rules! __impl_data_provider_never_marker {
    ($ty:path) => {
        impl<Y> $crate::DataProvider<$crate::marker::NeverMarker<Y>> for $ty
        where
            for<'a> Y: $crate::prelude::yoke::Yokeable<'a>,
        {
            fn load(
                &self,
                req: $crate::DataRequest,
            ) -> Result<$crate::DataResponse<$crate::marker::NeverMarker<Y>>, $crate::DataError>
            {
                Err($crate::DataErrorKind::MarkerNotFound.with_req(
                    <$crate::marker::NeverMarker<Y> as $crate::DataMarker>::INFO,
                    req,
                ))
            }
        }
    };
}
#[doc(inline)]
pub use __impl_data_provider_never_marker as impl_data_provider_never_marker;

#[doc(hidden)] // macro
#[macro_export]
macro_rules! leading_tag {
    () => {
        "\nicu4x_key_tag"
    };
}

#[doc(hidden)] // macro
#[macro_export]
macro_rules! trailing_tag {
    () => {
        "\n"
    };
}

#[doc(hidden)] // macro
#[macro_export]
macro_rules! tagged {
    ($without_tags:expr) => {
        concat!(
            $crate::leading_tag!(),
            $without_tags,
            $crate::trailing_tag!()
        )
    };
}

/// A compact hash of a [`DataMarkerInfo`]. Useful for keys in maps.
///
/// The hash will be stable over time within major releases.
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Copy, Clone, Hash, ULE)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct DataMarkerPathHash([u8; 4]);

impl DataMarkerPathHash {
    /// Gets the hash value as a byte array.
    pub const fn to_bytes(self) -> [u8; 4] {
        self.0
    }
}

/// Const function to compute the FxHash of a byte array.
///
/// FxHash is a speedy hash algorithm used within rustc. The algorithm is satisfactory for our
/// use case since the strings being hashed originate from a trusted source (the ICU4X
/// components), and the hashes are computed at compile time, so we can check for collisions.
///
/// We could have considered a SHA or other cryptographic hash function. However, we are using
/// FxHash because:
///
/// 1. There is precedent for this algorithm in Rust
/// 2. The algorithm is easy to implement as a const function
/// 3. The amount of code is small enough that we can reasonably keep the algorithm in-tree
/// 4. FxHash is designed to output 32-bit or 64-bit values, whereas SHA outputs more bits,
///    such that truncation would be required in order to fit into a u32, partially reducing
///    the benefit of a cryptographically secure algorithm
// The indexing operations in this function have been reviewed in detail and won't panic.
#[allow(clippy::indexing_slicing)]
const fn fxhash_32(bytes: &[u8], ignore_leading: usize, ignore_trailing: usize) -> u32 {
    // This code is adapted from https://github.com/rust-lang/rustc-hash,
    // whose license text is reproduced below.
    //
    // Copyright 2015 The Rust Project Developers. See the COPYRIGHT
    // file at the top-level directory of this distribution and at
    // http://rust-lang.org/COPYRIGHT.
    //
    // Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
    // http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
    // <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
    // option. This file may not be copied, modified, or distributed
    // except according to those terms.

    if ignore_leading + ignore_trailing >= bytes.len() {
        return 0;
    }

    #[inline]
    const fn hash_word_32(mut hash: u32, word: u32) -> u32 {
        const ROTATE: u32 = 5;
        const SEED32: u32 = 0x9e_37_79_b9;
        hash = hash.rotate_left(ROTATE);
        hash ^= word;
        hash = hash.wrapping_mul(SEED32);
        hash
    }

    let mut cursor = ignore_leading;
    let end = bytes.len() - ignore_trailing;
    let mut hash = 0;

    while end - cursor >= 4 {
        let word = u32::from_le_bytes([
            bytes[cursor],
            bytes[cursor + 1],
            bytes[cursor + 2],
            bytes[cursor + 3],
        ]);
        hash = hash_word_32(hash, word);
        cursor += 4;
    }

    if end - cursor >= 2 {
        let word = u16::from_le_bytes([bytes[cursor], bytes[cursor + 1]]);
        hash = hash_word_32(hash, word as u32);
        cursor += 2;
    }

    if end - cursor >= 1 {
        hash = hash_word_32(hash, bytes[cursor] as u32);
    }

    hash
}

impl<'a> zerovec::maps::ZeroMapKV<'a> for DataMarkerPathHash {
    type Container = zerovec::ZeroVec<'a, DataMarkerPathHash>;
    type Slice = zerovec::ZeroSlice<DataMarkerPathHash>;
    type GetType = <DataMarkerPathHash as AsULE>::ULE;
    type OwnedType = DataMarkerPathHash;
}

impl AsULE for DataMarkerPathHash {
    type ULE = Self;
    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        self
    }
    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        unaligned
    }
}

// Safe since the ULE type is `self`.
unsafe impl EqULE for DataMarkerPathHash {}

/// The string path of a data marker. For example, "foo@1"
///
/// ```
/// # use icu_provider::marker::DataMarkerPath;
/// const K: DataMarkerPath =
///     icu_provider::marker::data_marker_path!("foo/bar@1");
/// ```
///
/// The human-readable path string ends with `@` followed by one or more digits (the version
/// number). Paths do not contain characters other than ASCII letters and digits, `_`, `/`.
///
/// Invalid paths are compile-time errors (as [`data_marker_path!`](crate::marker::data_marker_path) uses `const`).
///
/// ```compile_fail,E0080
/// # use icu_provider::marker::DataMarkerPath;
/// const K: DataMarkerPath = icu_provider::marker::data_marker_path!("foo/../bar@1");
/// ```
#[derive(Debug, Copy, Clone, Eq)]
pub struct DataMarkerPath {
    // This string literal is wrapped in leading_tag!() and trailing_tag!() to make it detectable
    // in a compiled binary.
    tagged: &'static str,
    hash: DataMarkerPathHash,
}

impl PartialEq for DataMarkerPath {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.hash == other.hash && self.tagged == other.tagged
    }
}

impl Ord for DataMarkerPath {
    #[inline]
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.tagged.cmp(other.tagged)
    }
}

impl PartialOrd for DataMarkerPath {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.tagged.cmp(other.tagged))
    }
}

impl core::hash::Hash for DataMarkerPath {
    #[inline]
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.hash.hash(state)
    }
}

impl DataMarkerPath {
    #[doc(hidden)]
    // macro use
    // Error is a str of the expected character class and the index where it wasn't encountered
    // The indexing operations in this function have been reviewed in detail and won't panic.
    #[allow(clippy::indexing_slicing)]
    pub const fn construct_internal(tagged: &'static str) -> Result<Self, (&'static str, usize)> {
        if tagged.len() < leading_tag!().len() + trailing_tag!().len() {
            return Err(("tag", 0));
        }
        // Start and end of the untagged part
        let start = leading_tag!().len();
        let end = tagged.len() - trailing_tag!().len();

        // Check tags
        let mut i = 0;
        while i < leading_tag!().len() {
            if tagged.as_bytes()[i] != leading_tag!().as_bytes()[i] {
                return Err(("tag", 0));
            }
            i += 1;
        }
        i = 0;
        while i < trailing_tag!().len() {
            if tagged.as_bytes()[end + i] != trailing_tag!().as_bytes()[i] {
                return Err(("tag", end + 1));
            }
            i += 1;
        }

        match Self::validate_path_manual_slice(tagged, start, end) {
            Ok(()) => (),
            Err(e) => return Err(e),
        };

        let hash = DataMarkerPathHash(
            fxhash_32(
                tagged.as_bytes(),
                leading_tag!().len(),
                trailing_tag!().len(),
            )
            .to_le_bytes(),
        );

        Ok(Self { tagged, hash })
    }

    const fn validate_path_manual_slice(
        path: &'static str,
        start: usize,
        end: usize,
    ) -> Result<(), (&'static str, usize)> {
        debug_assert!(start <= end);
        debug_assert!(end <= path.len());
        // Regex: [a-zA-Z0-9_][a-zA-Z0-9_/]*@[0-9]+
        enum State {
            Empty,
            Body,
            At,
            Version,
        }
        use State::*;
        let mut i = start;
        let mut state = Empty;
        loop {
            let byte = if i < end {
                #[allow(clippy::indexing_slicing)] // protected by debug assertion
                Some(path.as_bytes()[i])
            } else {
                None
            };
            state = match (state, byte) {
                (Empty | Body, Some(b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'_')) => Body,
                (Body, Some(b'/')) => Body,
                (Body, Some(b'@')) => At,
                (At | Version, Some(b'0'..=b'9')) => Version,
                // One of these cases will be hit at the latest when i == end, so the loop converges.
                (Version, None) => {
                    return Ok(());
                }

                (Empty, _) => return Err(("[a-zA-Z0-9_]", i)),
                (Body, _) => return Err(("[a-zA-z0-9_/@]", i)),
                (At, _) => return Err(("[0-9]", i)),
                (Version, _) => return Err(("[0-9]", i)),
            };
            i += 1;
        }
    }

    /// Gets the path as a static string slice.
    #[inline]
    pub const fn as_str(self) -> &'static str {
        unsafe {
            // Safe due to invariant that self.path is tagged correctly
            core::str::from_utf8_unchecked(core::slice::from_raw_parts(
                self.tagged.as_ptr().add(leading_tag!().len()),
                self.tagged.len() - trailing_tag!().len() - leading_tag!().len(),
            ))
        }
    }

    /// Gets a platform-independent hash of a [`DataMarkerPath`].
    ///
    /// The hash is 4 bytes and allows for fast comparison.
    ///
    /// # Example
    ///
    /// ```
    /// use icu_provider::marker::DataMarkerPath;
    /// use icu_provider::marker::DataMarkerPathHash;
    ///
    /// const PATH: DataMarkerPath =
    ///     icu_provider::marker::data_marker_path!("foo@1");
    /// const PATH_HASH: DataMarkerPathHash = PATH.hashed();
    ///
    /// assert_eq!(PATH_HASH.to_bytes(), [0xe2, 0xb6, 0x17, 0x71]);
    /// ```
    #[inline]
    pub const fn hashed(self) -> DataMarkerPathHash {
        self.hash
    }
}

/// Used for loading data from a dynamic ICU4X data provider.
///
/// A data marker is tightly coupled with the code that uses it to load data at runtime.
/// Executables can be searched for `DataMarkerInfo` instances to produce optimized data files.
/// Therefore, users should not generally create DataMarkerInfo instances; they should instead use
/// the ones exported by a component.
#[derive(Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub struct DataMarkerInfo {
    /// The human-readable path string ends with `@` followed by one or more digits (the version
    /// number). Paths do not contain characters other than ASCII letters and digits, `_`, `/`.
    ///
    /// Useful for reading and writing data to a file system.
    pub path: DataMarkerPath,
    /// TODO
    pub attributes_domain: &'static str,
    /// TODO
    pub is_singleton: bool,
    /// TODO
    pub fallback_config: LocaleFallbackConfig,
}

impl PartialOrd for DataMarkerInfo {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.path.cmp(&other.path))
    }
}

impl Ord for DataMarkerInfo {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.path.cmp(&other.path)
    }
}

impl core::hash::Hash for DataMarkerInfo {
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.path.hash(state)
    }
}

impl DataMarkerInfo {
    /// See [`Default::default`]
    pub const fn from_path(path: DataMarkerPath) -> Self {
        Self {
            path,
            is_singleton: false,
            attributes_domain: "",
            fallback_config: LocaleFallbackConfig::default(),
        }
    }

    /// Returns [`Ok`] if this data marker matches the argument, or the appropriate error.
    ///
    /// Convenience method for data providers that support a single [`DataMarkerInfo`].
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::prelude::*;
    /// use icu_provider::hello_world::*;
    /// # struct DummyMarker;
    /// # impl DynamicDataMarker for DummyMarker {
    /// #     type DataStruct = <HelloWorldV1Marker as DynamicDataMarker>::DataStruct;
    /// # }
    /// # impl DataMarker for DummyMarker {
    /// #     const INFO: DataMarkerInfo = DataMarkerInfo::from_path(icu_provider::marker::data_marker_path!("dummy@1"));
    /// # }
    ///
    /// assert!(matches!(HelloWorldV1Marker::INFO.match_marker(HelloWorldV1Marker::INFO), Ok(())));
    /// assert!(matches!(
    ///     HelloWorldV1Marker::INFO.match_marker(DummyMarker::INFO),
    ///     Err(DataError {
    ///         kind: DataErrorKind::MarkerNotFound,
    ///         ..
    ///     })
    /// ));
    ///
    /// // The error context contains the argument:
    /// assert_eq!(HelloWorldV1Marker::INFO.match_marker(DummyMarker::INFO).unwrap_err().marker_path, Some(DummyMarker::INFO.path));
    /// ```
    pub fn match_marker(self, marker: Self) -> Result<(), DataError> {
        if self == marker {
            Ok(())
        } else {
            Err(DataErrorKind::MarkerNotFound.with_marker(marker))
        }
    }

    /// Constructs a [`DataLocale`] for this [`DataMarkerInfo`].
    pub fn make_locale(
        self,
        locale: icu_locale_core::preferences::LocalePreferences,
    ) -> DataLocale {
        DataLocale::from_preferences_with_info(locale, self)
    }
}

/// See [`DataMarkerInfo`].
#[doc(hidden)] // macro
#[macro_export]
macro_rules! __data_marker_path {
    ($path:expr) => {{
        // Force the DataMarkerInfo into a const context
        const X: $crate::marker::DataMarkerPath =
            match $crate::marker::DataMarkerPath::construct_internal($crate::tagged!($path)) {
                Ok(path) => path,
                #[allow(clippy::panic)] // Const context
                Err(_) => panic!(concat!("Invalid path: ", $path)),
                // TODO Once formatting is const:
                // Err((expected, index)) => panic!(
                //     "Invalid resource key {:?}: expected {:?}, found {:?} ",
                //     $path,
                //     expected,
                //     $crate::tagged!($path).get(index..))
                // );
            };
        X
    }};
}
#[doc(inline)]
pub use __data_marker_path as data_marker_path;

impl fmt::Debug for DataMarkerInfo {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.path.as_str())
    }
}

/// A marker for the given `DataStruct`.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct ErasedMarker<DataStruct: for<'a> Yokeable<'a>>(PhantomData<DataStruct>);
impl<DataStruct: for<'a> Yokeable<'a>> DynamicDataMarker for ErasedMarker<DataStruct> {
    type DataStruct = DataStruct;
}

#[test]
fn test_path_syntax() {
    // Valid paths:
    DataMarkerPath::construct_internal(tagged!("hello/world@1")).unwrap();
    DataMarkerPath::construct_internal(tagged!("hello/world/foo@1")).unwrap();
    DataMarkerPath::construct_internal(tagged!("hello/world@999")).unwrap();
    DataMarkerPath::construct_internal(tagged!("hello_world/foo@1")).unwrap();
    DataMarkerPath::construct_internal(tagged!("hello_458/world@1")).unwrap();
    DataMarkerPath::construct_internal(tagged!("hello_world@1")).unwrap();

    // No version:
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("hello/world"),),
        Err((
            "[a-zA-z0-9_/@]",
            concat!(leading_tag!(), "hello/world").len()
        ))
    );

    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("hello/world@"),),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("hello/world@foo"),),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("hello/world@1foo"),),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@1").len()))
    );

    // Meta no longer accepted:
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[R]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[u-ca]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[R][u-ca]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );

    // Invalid meta:
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[U]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[uca]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[u-"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[u-caa]"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("foo@1[R"),),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );

    // Invalid characters:
    assert_eq!(
        DataMarkerPath::construct_internal(tagged!("你好/世界@1"),),
        Err(("[a-zA-Z0-9_]", leading_tag!().len()))
    );

    // Invalid tag:
    assert_eq!(
        DataMarkerPath::construct_internal(concat!("hello/world@1", trailing_tag!()),),
        Err(("tag", 0))
    );
    assert_eq!(
        DataMarkerPath::construct_internal(concat!(leading_tag!(), "hello/world@1"),),
        Err(("tag", concat!(leading_tag!(), "hello/world@1").len()))
    );
    assert_eq!(
        DataMarkerPath::construct_internal("hello/world@1",),
        Err(("tag", 0))
    );
}

#[test]
fn test_path_to_string() {
    struct TestCase {
        pub path: DataMarkerPath,
        pub expected: &'static str,
    }

    for cas in [
        TestCase {
            path: data_marker_path!("core/cardinal@1"),
            expected: "core/cardinal@1",
        },
        TestCase {
            path: data_marker_path!("core/maxlengthsubcatg@1"),
            expected: "core/maxlengthsubcatg@1",
        },
        TestCase {
            path: data_marker_path!("core/cardinal@65535"),
            expected: "core/cardinal@65535",
        },
    ] {
        assert_eq!(cas.expected, cas.path.as_str());
    }
}

#[test]
fn test_hash_word_32() {
    assert_eq!(0, fxhash_32(b"", 0, 0));
    assert_eq!(0, fxhash_32(b"a", 1, 0));
    assert_eq!(0, fxhash_32(b"a", 0, 1));
    assert_eq!(0, fxhash_32(b"a", 0, 10));
    assert_eq!(0, fxhash_32(b"a", 10, 0));
    assert_eq!(0, fxhash_32(b"a", 1, 1));
    assert_eq!(0xF3051F19, fxhash_32(b"a", 0, 0));
    assert_eq!(0x2F9DF119, fxhash_32(b"ab", 0, 0));
    assert_eq!(0xCB1D9396, fxhash_32(b"abc", 0, 0));
    assert_eq!(0x8628F119, fxhash_32(b"abcd", 0, 0));
    assert_eq!(0xBEBDB56D, fxhash_32(b"abcde", 0, 0));
    assert_eq!(0x1CE8476D, fxhash_32(b"abcdef", 0, 0));
    assert_eq!(0xC0F176A4, fxhash_32(b"abcdefg", 0, 0));
    assert_eq!(0x09AB476D, fxhash_32(b"abcdefgh", 0, 0));
    assert_eq!(0xB72F5D88, fxhash_32(b"abcdefghi", 0, 0));
}

#[test]
fn test_path_hash() {
    struct TestCase {
        pub path: DataMarkerPath,
        pub hash: DataMarkerPathHash,
    }

    for cas in [
        TestCase {
            path: data_marker_path!("core/cardinal@1"),
            hash: DataMarkerPathHash([172, 207, 42, 236]),
        },
        TestCase {
            path: data_marker_path!("core/maxlengthsubcatg@1"),
            hash: DataMarkerPathHash([193, 6, 79, 61]),
        },
        TestCase {
            path: data_marker_path!("core/cardinal@65535"),
            hash: DataMarkerPathHash([176, 131, 182, 223]),
        },
    ] {
        assert_eq!(cas.hash, cas.path.hashed(), "{}", cas.path.as_str());
    }
}
