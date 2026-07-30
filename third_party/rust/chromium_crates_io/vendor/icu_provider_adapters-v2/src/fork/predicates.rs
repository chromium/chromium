// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Collection of predicate traits and functions for forking providers.

use icu_provider::prelude::*;

/// The predicate trait used by [`ForkByErrorProvider`].
///
/// [`ForkByErrorProvider`]: super::ForkByErrorProvider
pub trait ForkByErrorPredicate {
    /// The error to return if there are zero providers.
    const UNIT_ERROR: DataErrorKind = DataErrorKind::MarkerNotFound;

    /// This function is called when a data request fails and there are additional providers
    /// that could possibly fulfill the request.
    ///
    /// Arguments:
    ///
    /// - `&self` = Reference to the struct implementing the trait (for data capture)
    /// - `marker` = The [`DataMarkerInfo`] associated with the request
    /// - `req` = The [`DataRequest`]. This may be `None` if there is no request, such as
    ///   inside [`IterableDynamicDataProvider`].
    /// - `err` = The error that occurred.
    ///
    /// Return value:
    ///
    /// - `true` to discard the error and attempt the request with the next provider.
    /// - `false` to return the error and not perform any additional requests.
    fn test(&self, marker: DataMarkerInfo, req: Option<DataRequest>, err: DataError) -> bool;
}

/// A predicate that allows forking providers to search for a provider that supports a
/// particular data marker.
///
/// This is normally used implicitly by [`ForkByMarkerProvider`].
///
/// [`ForkByMarkerProvider`]: super::ForkByMarkerProvider
#[derive(Debug, PartialEq, Eq)]
#[non_exhaustive] // Not intended to be constructed
pub struct MarkerNotFoundPredicate;

impl ForkByErrorPredicate for MarkerNotFoundPredicate {
    const UNIT_ERROR: DataErrorKind = DataErrorKind::MarkerNotFound;

    #[inline]
    fn test(&self, _: DataMarkerInfo, _: Option<DataRequest>, err: DataError) -> bool {
        matches!(
            err,
            DataError {
                kind: DataErrorKind::MarkerNotFound,
                ..
            }
        )
    }
}

/// A predicate that allows forking providers to search for a provider that supports a
/// particular locale, based on whether it returns [`DataErrorKind::IdentifierNotFound`].
///
/// # Examples
///
/// ```
/// use icu_provider_adapters::fork::ForkByErrorProvider;
/// use icu_provider_adapters::fork::predicates::IdentifierNotFoundPredicate;
/// use icu_provider::prelude::*;
/// use icu_provider::hello_world::*;
/// use icu_locale::langid;
///
/// struct SingleLocaleProvider(DataLocale);
/// impl DataProvider<HelloWorldV1> for SingleLocaleProvider {
///     fn load(&self, req: DataRequest) -> Result<DataResponse<HelloWorldV1>, DataError> {
///         if *req.id.locale != self.0 {
///             return Err(DataErrorKind::IdentifierNotFound.with_req(HelloWorldV1::INFO, req));
///         }
///         HelloWorldProvider.load(req)
///     }
/// }
///
/// let provider_de = SingleLocaleProvider(langid!("de").into());
/// let provider_ro = SingleLocaleProvider(langid!("ro").into());
///
/// // Create the forking provider:
/// let provider = ForkByErrorProvider::new_with_predicate(
///     provider_de,
///     provider_ro,
///     IdentifierNotFoundPredicate
/// );
///
/// // Test that we can load both "de" and "ro" data:
///
/// let german_hello_world: DataResponse<HelloWorldV1> = provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("de").into()),
///         ..Default::default()
///     })
///     .expect("Loading should succeed");
///
/// assert_eq!("Hallo Welt", german_hello_world.payload.get().message);
///
/// let romanian_hello_world: DataResponse<HelloWorldV1> = provider
///     .load(DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("ro").into()),
///         ..Default::default()
///     })
///     .expect("Loading should succeed");
///
/// assert_eq!("Salut, lume", romanian_hello_world.payload.get().message);
///
/// // We should not be able to load "en" data because it is not in either provider:
///
/// DataProvider::<HelloWorldV1>::load(
///     &provider,
///     DataRequest {
///         id: DataIdentifierBorrowed::for_locale(&langid!("en").into()),
///         ..Default::default()
///     }
/// )
/// .expect_err("No English data");
/// ```
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // empty type
pub struct IdentifierNotFoundPredicate;

impl ForkByErrorPredicate for IdentifierNotFoundPredicate {
    const UNIT_ERROR: DataErrorKind = DataErrorKind::IdentifierNotFound;

    #[inline]
    fn test(&self, _: DataMarkerInfo, _: Option<DataRequest>, err: DataError) -> bool {
        Err::<(), _>(err).allow_identifier_not_found().is_ok()
    }
}
