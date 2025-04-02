// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
#[cfg(feature = "buffer_provider")]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_provider::buf::BufferProvider;

    use crate::errors::ffi::DataError;

    #[diplomat::opaque]
    /// An ICU4X data provider, capable of loading ICU4X data keys from some source.
    ///
    /// Currently the only source supported is loading from "blob" formatted data from a bytes buffer or the file system.
    ///
    /// If you wish to use ICU4X's builtin "compiled data", use the version of the constructors that do not have `_with_provider`
    /// in their names.
    #[diplomat::rust_link(icu_provider, Mod)]
    pub struct DataProvider(Option<Box<dyn BufferProvider + 'static>>);

    impl DataProvider {
        // These will be unused if almost *all* components are turned off, which is tedious and unproductive to gate for
        #[allow(unused)]
        pub(crate) fn get(
            &self,
        ) -> Result<&(dyn icu_provider::buf::BufferProvider + 'static), icu_provider::DataError>
        {
            match &self.0 {
                None => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                Some(ref buffer_provider) => Ok(buffer_provider),
            }
        }

        // These will be unused if almost *all* components are turned off, which is tedious and unproductive to gate for
        #[allow(unused)]
        pub(crate) fn get_unstable(
            &self,
        ) -> Result<
            icu_provider::buf::DeserializingBufferProvider<
                (dyn icu_provider::buf::BufferProvider + 'static),
            >,
            icu_provider::DataError,
        > {
            self.get()
                .map(icu_provider::buf::AsDeserializingBufferProvider::as_deserializing)
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
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        pub fn from_fs(path: &DiplomatStr) -> Result<Box<DataProvider>, DataError> {
            Ok(Box::new(DataProvider(Some(Box::new(
                icu_provider_fs::FsDataProvider::try_new(
                    // In the future we can start using OsString APIs to support non-utf8 paths
                    core::str::from_utf8(path)
                        .map_err(|_| DataError::Io)?
                        .into(),
                )?,
            )))))
        }

        /// Constructs a `BlobDataProvider` and returns it as an [`DataProvider`].
        #[diplomat::rust_link(icu_provider_blob::BlobDataProvider, Struct)]
        #[diplomat::attr(all(supports = fallible_constructors, supports = named_constructors), named_constructor)]
        #[diplomat::attr(not(supports = static_slices), disable)]
        pub fn from_byte_slice(
            blob: &'static [DiplomatByte],
        ) -> Result<Box<DataProvider>, DataError> {
            Ok(Box::new(DataProvider(Some(Box::new(
                icu_provider_blob::BlobDataProvider::try_new_from_static_blob(blob)?,
            )))))
        }

        /// Creates a provider that tries the current provider and then, if the current provider
        /// doesn't support the data key, another provider `other`.
        ///
        /// This takes ownership of the `other` provider, leaving an empty provider in its place.
        #[diplomat::rust_link(icu_provider_adapters::fork::ForkByMarkerProvider, Typedef)]
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::MarkerNotFoundPredicate,
            Struct,
            hidden
        )]
        pub fn fork_by_key(&mut self, other: &mut DataProvider) -> Result<(), DataError> {
            *self = match (core::mem::take(&mut self.0), core::mem::take(&mut other.0)) {
                (None, _) | (_, None) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                (Some(a), Some(b)) => DataProvider(Some(Box::new(
                    icu_provider_adapters::fork::ForkByMarkerProvider::new(a, b),
                ))),
            };
            Ok(())
        }

        /// Same as `fork_by_key` but forks by locale instead of key.
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::IdentifierNotFoundPredicate,
            Struct
        )]
        pub fn fork_by_locale(&mut self, other: &mut DataProvider) -> Result<(), DataError> {
            *self = match (core::mem::take(&mut self.0), core::mem::take(&mut other.0)) {
                (None, _) | (_, None) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                (Some(a), Some(b)) => DataProvider(Some(Box::new(
                    icu_provider_adapters::fork::ForkByErrorProvider::new_with_predicate(
                        a,
                        b,
                        icu_provider_adapters::fork::predicates::IdentifierNotFoundPredicate,
                    ),
                ))),
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
            *self = match core::mem::take(&mut self.0) {
                None => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                Some(inner) => DataProvider(Some(Box::new(
                    icu_provider_adapters::fallback::LocaleFallbackProvider::new(
                        inner,
                        fallbacker.0.clone(),
                    ),
                ))),
            };
            Ok(())
        }
    }
}
