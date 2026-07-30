// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! A data provider wrapper that performs locale fallback.

#[doc(no_inline)]
pub use icu_locale::LocaleFallbacker;
use icu_provider::prelude::*;
use icu_provider::DryDataProvider;
use icu_provider::DynamicDryDataProvider;

/// A data provider wrapper that performs locale fallback. This enables arbitrary locales to be
/// handled at runtime.
///
/// # Examples
///
/// ```
/// use icu_locale::langid;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::fallback::LocaleFallbackProvider;
///
/// let provider = HelloWorldProvider;
///
/// let id = DataIdentifierCow::from_locale(langid!("ja-JP").into());
///
/// // The provider does not have data for "ja-JP":
/// DataProvider::<HelloWorldV1>::load(
///     &provider,
///     DataRequest {
///         id: id.as_borrowed(),
///         ..Default::default()
///     },
/// )
/// .expect_err("No fallback");
///
/// // But if we wrap the provider in a fallback provider...
/// let provider = LocaleFallbackProvider::new(
///     provider,
///     icu_locale::LocaleFallbacker::new().static_to_owned(),
/// );
///
/// // ...then we can load "ja-JP" based on "ja" data
/// let response = DataProvider::<HelloWorldV1>::load(
///     &provider,
///     DataRequest {
///         id: id.as_borrowed(),
///         ..Default::default()
///     },
/// )
/// .expect("successful with vertical fallback");
///
/// assert_eq!(response.metadata.locale.unwrap(), langid!("ja").into(),);
/// assert_eq!(response.payload.get().message, "こんにちは世界",);
/// ```
#[derive(Clone, Debug)]
pub struct LocaleFallbackProvider<P> {
    inner: P,
    fallbacker: LocaleFallbacker,
}

impl<P> LocaleFallbackProvider<P> {
    /// Wraps a provider with a provider performing fallback under the given fallbacker.
    ///
    /// If the underlying provider contains deduplicated data, it is important to use the
    /// same fallback data that `ExportDriver` used.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locale::langid;
    /// use icu_locale::LocaleFallbacker;
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use icu_provider_adapters::fallback::LocaleFallbackProvider;
    ///
    /// let provider = HelloWorldProvider;
    ///
    /// let id = DataIdentifierCow::from_locale(langid!("de-CH").into());
    ///
    /// // There is no "de-CH" data in the `HelloWorldProvider`
    /// DataProvider::<HelloWorldV1>::load(
    ///     &provider,
    ///     DataRequest {
    ///         id: id.as_borrowed(),
    ///         ..Default::default()
    ///     },
    /// )
    /// .expect_err("No data for de-CH");
    ///
    /// // `HelloWorldProvider` does not contain fallback data,
    /// // but we can construct a fallbacker with `icu_locale`'s
    /// // compiled data.
    /// let provider = LocaleFallbackProvider::new(
    ///     provider,
    ///     LocaleFallbacker::new().static_to_owned(),
    /// );
    ///
    /// // Now we can load the "de-CH" data via fallback to "de".
    /// let german_hello_world: DataResponse<HelloWorldV1> = provider
    ///     .load(DataRequest {
    ///         id: id.as_borrowed(),
    ///         ..Default::default()
    ///     })
    ///     .expect("Loading should succeed");
    ///
    /// assert_eq!("Hallo Welt", german_hello_world.payload.get().message);
    /// ```
    pub fn new(provider: P, fallbacker: LocaleFallbacker) -> Self {
        Self {
            inner: provider,
            fallbacker,
        }
    }

    /// Returns a reference to the inner provider, bypassing fallback.
    pub fn inner(&self) -> &P {
        &self.inner
    }

    /// Returns a mutable reference to the inner provider.
    pub fn inner_mut(&mut self) -> &mut P {
        &mut self.inner
    }

    /// Returns ownership of the inner provider to the caller.
    pub fn into_inner(self) -> P {
        self.inner
    }

    /// Run the fallback algorithm with the data request using the inner data provider.
    /// Internal function; external clients should use one of the trait impls below.
    ///
    /// Function arguments:
    ///
    /// - F1 should perform a data load for a single [`DataRequest`] and return the result of it
    /// - F2 should map from the provider-specific response type to [`DataResponseMetadata`]
    fn run_fallback<F1, F2, R>(
        &self,
        marker: DataMarkerInfo,
        mut base_req: DataRequest,
        mut f1: F1,
        mut f2: F2,
    ) -> Result<R, DataError>
    where
        F1: FnMut(DataRequest) -> Result<R, DataError>,
        F2: FnMut(&mut R) -> &mut DataResponseMetadata,
    {
        if marker.is_singleton {
            return f1(base_req);
        }
        let mut fallback_iterator = self
            .fallbacker
            .for_config(marker.fallback_config)
            .fallback_for(*base_req.id.locale);
        let base_silent = core::mem::replace(&mut base_req.metadata.silent, true);
        loop {
            let result = f1(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes_and_locale(
                    base_req.id.marker_attributes,
                    fallback_iterator.get(),
                ),
                ..base_req
            });

            match result.allow_identifier_not_found() {
                Ok(Some(mut result)) => {
                    f2(&mut result).locale = Some(fallback_iterator.take());
                    return Ok(result);
                }
                Ok(None) => {
                    // If we just checked und, break out of the loop.
                    if fallback_iterator.get().is_unknown() {
                        break;
                    }
                    fallback_iterator.step();
                }
                Err(e) => {
                    // Log the original request rather than the fallback request
                    base_req.metadata.silent = base_silent;
                    return Err(e.with_req(marker, base_req));
                }
            };
        }
        base_req.metadata.silent = base_silent;
        Err(DataErrorKind::IdentifierNotFound.with_req(marker, base_req))
    }
}

impl<P, M> DynamicDataProvider<M> for LocaleFallbackProvider<P>
where
    P: DynamicDataProvider<M>,
    M: DynamicDataMarker,
{
    fn load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponse<M>, DataError> {
        self.run_fallback(
            marker,
            req,
            |req| self.inner.load_data(marker, req),
            |res| &mut res.metadata,
        )
    }
}

impl<P, M> DynamicDryDataProvider<M> for LocaleFallbackProvider<P>
where
    P: DynamicDryDataProvider<M>,
    M: DynamicDataMarker,
{
    fn dry_load_data(
        &self,
        marker: DataMarkerInfo,
        req: DataRequest,
    ) -> Result<DataResponseMetadata, DataError> {
        self.run_fallback(
            marker,
            req,
            |req| self.inner.dry_load_data(marker, req),
            |m| m,
        )
    }
}

impl<P, M> DataProvider<M> for LocaleFallbackProvider<P>
where
    P: DataProvider<M>,
    M: DataMarker,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.run_fallback(
            M::INFO,
            req,
            |req| self.inner.load(req),
            |res| &mut res.metadata,
        )
    }
}

impl<P, M> DryDataProvider<M> for LocaleFallbackProvider<P>
where
    P: DryDataProvider<M>,
    M: DataMarker,
{
    fn dry_load(&self, req: DataRequest) -> Result<DataResponseMetadata, DataError> {
        self.run_fallback(M::INFO, req, |req| self.inner.dry_load(req), |m| m)
    }
}

#[test]
fn dry_test() {
    use icu_provider::hello_world::*;

    struct TestProvider;
    impl DataProvider<HelloWorldV1> for TestProvider {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloWorldV1>, DataError> {
            panic!("pretend this is super expensive")
        }
    }

    impl DryDataProvider<HelloWorldV1> for TestProvider {
        fn dry_load(&self, req: DataRequest) -> Result<DataResponseMetadata, DataError> {
            // We support all languages except English, and no regional variants. This is cheap to check.
            if req.id.locale.region.is_some() || req.id.locale.language.as_str() == "en" {
                Err(DataErrorKind::IdentifierNotFound.into_error())
            } else {
                Ok(Default::default())
            }
        }
    }

    let provider =
        LocaleFallbackProvider::new(TestProvider, LocaleFallbacker::new().static_to_owned());

    assert_eq!(
        provider
            .dry_load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&"de-CH".parse().unwrap()),
                ..Default::default()
            })
            .unwrap()
            .locale,
        "de".parse::<DataLocale>().ok()
    );

    assert_eq!(
        provider
            .dry_load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&"en-GB".parse().unwrap()),
                ..Default::default()
            })
            .unwrap()
            .locale,
        Some(DataLocale::default())
    );
}
