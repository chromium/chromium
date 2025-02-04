// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_provider::prelude::*;

pub enum DataProviderInner {
    Destroyed,
    Empty,
    #[cfg(feature = "compiled_data")]
    Compiled,
    #[cfg(feature = "buffer_provider")]
    Buffer(alloc::boxed::Box<dyn BufferProvider + 'static>),
}

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use alloc::boxed::Box;

    use super::DataProviderInner;
    use crate::errors::ffi::DataError;

    #[diplomat::opaque]
    /// An ICU4X data provider, capable of loading ICU4X data keys from some source.
    #[diplomat::rust_link(icu_provider, Mod)]
    pub struct DataProvider(pub DataProviderInner);

    #[cfg(feature = "buffer_provider")]
    fn convert_buffer_provider<D: icu_provider::buf::BufferProvider + 'static>(
        x: D,
    ) -> DataProvider {
        DataProvider(super::DataProviderInner::Buffer(Box::new(x)))
    }

    impl DataProvider {
        /// Constructs an [`DataProvider`] that uses compiled data.
        ///
        /// Requires the `compiled_data` feature.
        ///
        /// This provider cannot be modified or combined with other providers, so `enable_fallback`,
        /// `enabled_fallback_with`, `fork_by_locale`, and `fork_by_key` will return `Err`s.
        #[cfg(feature = "compiled_data")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[diplomat::demo(default_constructor)]
        pub fn compiled() -> Box<DataProvider> {
            Box::new(Self(DataProviderInner::Compiled))
        }

        /// Constructs an `FsDataProvider` and returns it as an [`DataProvider`].
        /// Requires the `provider_fs` Cargo feature.
        /// Not supported in WASM.
        #[diplomat::rust_link(icu_provider_fs::FsDataProvider, Struct)]
        #[cfg(all(
            feature = "provider_fs",
            not(any(target_arch = "wasm32", target_os = "none"))
        ))]
        #[diplomat::attr(any(dart, js), disable)]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn from_fs(path: &DiplomatStr) -> Result<Box<DataProvider>, DataError> {
            Ok(Box::new(convert_buffer_provider(
                icu_provider_fs::FsDataProvider::try_new(
                    // In the future we can start using OsString APIs to support non-utf8 paths
                    core::str::from_utf8(path)
                        .map_err(|_| DataError::Io)?
                        .into(),
                )?,
            )))
        }

        /// Constructs a `BlobDataProvider` and returns it as an [`DataProvider`].
        #[diplomat::rust_link(icu_provider_blob::BlobDataProvider, Struct)]
        #[cfg(feature = "buffer_provider")]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        #[diplomat::attr(not(supports = static_slices), disable)]
        pub fn from_byte_slice(
            blob: &'static [DiplomatByte],
        ) -> Result<Box<DataProvider>, DataError> {
            Ok(Box::new(convert_buffer_provider(
                icu_provider_blob::BlobDataProvider::try_new_from_static_blob(blob)?,
            )))
        }

        /// Constructs an empty [`DataProvider`].
        #[diplomat::rust_link(icu_provider_adapters::empty::EmptyDataProvider, Struct)]
        #[diplomat::rust_link(
            icu_provider_adapters::empty::EmptyDataProvider::new,
            FnInStruct,
            hidden
        )]
        #[diplomat::attr(supports = fallible_constructors, named_constructor)]
        pub fn empty() -> Box<DataProvider> {
            Box::new(DataProvider(DataProviderInner::Empty))
        }

        /// Creates a provider that tries the current provider and then, if the current provider
        /// doesn't support the data key, another provider `other`.
        ///
        /// This takes ownership of the `other` provider, leaving an empty provider in its place.
        ///
        /// The providers must be the same type (Any or Buffer). This condition is satisfied if
        /// both providers originate from the same constructor, such as `create_from_byte_slice`
        /// or `create_fs`. If the condition is not upheld, a runtime error occurs.
        #[diplomat::rust_link(icu_provider_adapters::fork::ForkByMarkerProvider, Typedef)]
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::MarkerNotFoundPredicate,
            Struct,
            hidden
        )]
        pub fn fork_by_key(&mut self, other: &mut DataProvider) -> Result<(), DataError> {
            #[allow(unused_imports)]
            use DataProviderInner::*;
            *self = match (
                core::mem::replace(&mut self.0, Destroyed),
                core::mem::replace(&mut other.0, Destroyed),
            ) {
                (Destroyed, _) | (_, Destroyed) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                #[cfg(feature = "compiled_data")]
                (Compiled, _) | (_, Compiled) => Err(icu_provider::DataError::custom(
                    "The compiled provider cannot be modified",
                ))?,
                (Empty, Empty) => DataProvider(DataProviderInner::Empty),
                #[cfg(feature = "buffer_provider")]
                (Empty, b) | (b, Empty) => DataProvider(b),
                #[cfg(feature = "buffer_provider")]
                (Buffer(a), Buffer(b)) => convert_buffer_provider(
                    icu_provider_adapters::fork::ForkByMarkerProvider::new(a, b),
                ),
            };
            Ok(())
        }

        /// Same as `fork_by_key` but forks by locale instead of key.
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::IdentifierNotFoundPredicate,
            Struct
        )]
        pub fn fork_by_locale(&mut self, other: &mut DataProvider) -> Result<(), DataError> {
            #[allow(unused_imports)]
            use DataProviderInner::*;
            *self = match (
                core::mem::replace(&mut self.0, Destroyed),
                core::mem::replace(&mut other.0, Destroyed),
            ) {
                (Destroyed, _) | (_, Destroyed) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                #[cfg(feature = "compiled_data")]
                (Compiled, _) | (_, Compiled) => Err(icu_provider::DataError::custom(
                    "The compiled provider cannot be modified",
                ))?,
                (Empty, Empty) => DataProvider(DataProviderInner::Empty),
                #[cfg(feature = "buffer_provider")]
                (Empty, b) | (b, Empty) => DataProvider(b),
                #[cfg(feature = "buffer_provider")]
                (Buffer(a), Buffer(b)) => convert_buffer_provider(
                    icu_provider_adapters::fork::ForkByErrorProvider::new_with_predicate(
                        a,
                        b,
                        icu_provider_adapters::fork::predicates::IdentifierNotFoundPredicate,
                    ),
                ),
            };
            Ok(())
        }

        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider::new,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider,
            Struct,
            compact
        )]
        #[allow(unused_variables)] // feature-gated
        #[cfg(feature = "locale")]
        pub fn enable_locale_fallback_with(
            &mut self,
            fallbacker: &crate::fallbacker::ffi::LocaleFallbacker,
        ) -> Result<(), DataError> {
            use DataProviderInner::*;
            *self = match core::mem::replace(&mut self.0, Destroyed) {
                Destroyed => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                #[cfg(feature = "compiled_data")]
                Compiled => Err(icu_provider::DataError::custom(
                    "The compiled provider cannot be modified",
                ))?,
                Empty => Err(icu_provider::DataErrorKind::MarkerNotFound.into_error())?,
                #[cfg(feature = "buffer_provider")]
                Buffer(inner) => convert_buffer_provider(
                    icu_provider_adapters::fallback::LocaleFallbackProvider::new(
                        inner,
                        fallbacker.0.clone(),
                    ),
                ),
            };
            Ok(())
        }
    }
}

macro_rules! load {
    () => {
        fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
            use DataProviderInner::*;
            match self {
                Destroyed => Err(DataError::custom("This provider has been destroyed"))?,
                Empty => icu_provider_adapters::empty::EmptyDataProvider::new().load(req),
                #[cfg(feature = "buffer_provider")]
                Buffer(buffer_provider) => buffer_provider.as_deserializing().load(req),
                #[cfg(feature = "compiled_data")]
                Compiled => unreachable!(),
            }
        }
    };
}

#[cfg(not(feature = "buffer_provider"))]
impl<M> DataProvider<M> for DataProviderInner
where
    M: DataMarker,
{
    load!();
}

#[cfg(feature = "buffer_provider")]
impl<M> DataProvider<M> for DataProviderInner
where
    M: DataMarker,
    // Actual bound:
    //     for<'de> <M::DataStruct as DataStruct<'de>>::Output: Deserialize<'de>,
    // Necessary workaround bound (see `yoke::trait_hack` docs):
    for<'de> yoke::trait_hack::YokeTraitHack<<M::DataStruct as yoke::Yokeable<'de>>::Output>:
        serde::Deserialize<'de>,
{
    load!();
}

#[macro_export]
macro_rules! call_constructor {
    ($compiled:path [$pre_transform:ident => $transform:expr], $any:path, $buffer:path, $provider:expr $(, $args:expr)* $(,)?) => {
        match &$provider.0 {
            $crate::provider::DataProviderInner::Destroyed => Err(icu_provider::DataError::custom(
                "This provider has been destroyed",
            ))?,
            $crate::provider::DataProviderInner::Empty => $any(&icu_provider_adapters::empty::EmptyDataProvider::new(), $($args,)*),
            #[cfg(feature = "buffer_provider")]
            $crate::provider::DataProviderInner::Buffer(buffer_provider) => $buffer(buffer_provider, $($args,)*),
            #[cfg(feature = "compiled_data")]
            $crate::provider::DataProviderInner::Compiled => { let $pre_transform = $compiled($($args,)*); $transform },
        }
    };
    ($compiled:path, $any:path, $buffer:path, $provider:expr $(, $args:expr)* $(,)?) => {
        match &$provider.0 {
            $crate::provider::DataProviderInner::Destroyed => Err(icu_provider::DataError::custom(
                "This provider has been destroyed",
            ))?,
            $crate::provider::DataProviderInner::Empty => $any(&icu_provider_adapters::empty::EmptyDataProvider::new(), $($args,)*),
            #[cfg(feature = "buffer_provider")]
            $crate::provider::DataProviderInner::Buffer(buffer_provider) => $buffer(buffer_provider, $($args,)*),
            #[cfg(feature = "compiled_data")]
            $crate::provider::DataProviderInner::Compiled => $compiled($($args,)*),
        }
    };
}

#[macro_export]
macro_rules! call_constructor_unstable {
    ($compiled:path [$pre_transform:ident => $transform:expr], $unstable:path, $provider:expr $(, $args:expr)* $(,)?) => {
        match &$provider.0 {
            $crate::provider::DataProviderInner::Destroyed => Err(icu_provider::DataError::custom(
                "This provider has been destroyed",
            ))?,
            $crate::provider::DataProviderInner::Empty => $unstable(&icu_provider_adapters::empty::EmptyDataProvider::new(), $($args,)*),
            #[cfg(feature = "buffer_provider")]
            $crate::provider::DataProviderInner::Buffer(buffer_provider) => $unstable(&icu_provider::buf::AsDeserializingBufferProvider::as_deserializing(buffer_provider), $($args,)*),
            #[cfg(feature = "compiled_data")]
            $crate::provider::DataProviderInner::Compiled => { let $pre_transform = $compiled($($args,)*); $transform },
        }
    };
    ($compiled:path, $unstable:path, $provider:expr $(, $args:expr)* $(,)?) => {
        match &$provider.0 {
            $crate::provider::DataProviderInner::Destroyed => Err(icu_provider::DataError::custom(
                "This provider has been destroyed",
            ))?,
            $crate::provider::DataProviderInner::Empty => $unstable(&icu_provider_adapters::empty::EmptyDataProvider::new(), $($args,)*),
            #[cfg(feature = "buffer_provider")]
            $crate::provider::DataProviderInner::Buffer(buffer_provider) => $unstable(&icu_provider::buf::AsDeserializingBufferProvider::as_deserializing(buffer_provider), $($args,)*),
            #[cfg(feature = "compiled_data")]
            $crate::provider::DataProviderInner::Compiled => $compiled($($args,)*),
        }
    };
}
