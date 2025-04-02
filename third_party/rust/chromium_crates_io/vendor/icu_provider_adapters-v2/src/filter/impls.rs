// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::*;
use alloc::boxed::Box;
use icu_provider::prelude::*;

impl<D> FilterDataProvider<D, fn(DataIdentifierBorrowed) -> bool> {
    /// Creates a [`FilterDataProvider`] that does not do any filtering.
    ///
    /// Filters can be added using [`Self::with_filter`].
    pub fn new(provider: D, filter_name: &'static str) -> Self {
        Self {
            inner: provider,
            predicate: |_| true,
            filter_name,
        }
    }
}

impl<D, F> FilterDataProvider<D, F>
where
    F: Fn(DataIdentifierBorrowed) -> bool + Sync,
{
    /// Filter out data requests with certain langids according to the predicate function. The
    /// predicate should return `true` to allow a langid and `false` to reject a langid.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locale::LanguageIdentifier;
    /// use icu_locale::{langid, subtags::language};
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use icu_provider_adapters::filter::FilterDataProvider;
    ///
    /// let provider =
    ///     FilterDataProvider::new(HelloWorldProvider, "Demo no-English filter")
    ///         .with_filter(|id| id.locale.language != language!("en"));
    ///
    /// // German requests should succeed:
    /// let de = DataIdentifierCow::from_locale(langid!("de").into());
    /// let response: Result<DataResponse<HelloWorldV1>, _> =
    ///     provider.load(DataRequest {
    ///         id: de.as_borrowed(),
    ///         ..Default::default()
    ///     });
    /// assert!(matches!(response, Ok(_)));
    ///
    /// // English requests should fail:
    /// let en = DataIdentifierCow::from_locale(langid!("en-US").into());
    /// let response: Result<DataResponse<HelloWorldV1>, _> =
    ///     provider.load(DataRequest {
    ///         id: en.as_borrowed(),
    ///         ..Default::default()
    ///     });
    /// let response: Result<DataResponse<HelloWorldV1>, _> =
    ///     provider.load(DataRequest {
    ///         id: en.as_borrowed(),
    ///         ..Default::default()
    ///     });
    /// assert!(matches!(
    ///     response,
    ///     Err(DataError {
    ///         kind: DataErrorKind::IdentifierNotFound,
    ///         ..
    ///     })
    /// ));
    ///
    /// // English should not appear in the iterator result:
    /// let available_ids = provider
    ///     .iter_ids()
    ///     .expect("Should successfully make an iterator of supported locales");
    /// assert!(available_ids
    ///     .contains(&DataIdentifierCow::from_locale(langid!("de").into())));
    /// assert!(!available_ids
    ///     .contains(&DataIdentifierCow::from_locale(langid!("en").into())));
    /// ```
    #[allow(clippy::type_complexity)]
    pub fn with_filter<'a>(
        self,
        predicate: impl Fn(DataIdentifierBorrowed) -> bool + Sync + 'a,
    ) -> FilterDataProvider<D, Box<dyn Fn(DataIdentifierBorrowed) -> bool + Sync + 'a>>
    where
        F: 'a,
    {
        let old_predicate = self.predicate;
        FilterDataProvider {
            inner: self.inner,
            predicate: Box::new(move |id| -> bool {
                if !(old_predicate)(id) {
                    return false;
                }
                predicate(id)
            }),
            filter_name: self.filter_name,
        }
    }
}
