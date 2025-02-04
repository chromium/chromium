// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// Make a forking data provider with an arbitrary number of inner providers
/// that are known at build time.
///
/// # Examples
///
/// ```
/// use icu_provider_adapters::fork::ForkByMarkerProvider;
///
/// // Some empty example providers:
/// #[derive(PartialEq, Debug)]
/// struct Provider1;
/// #[derive(PartialEq, Debug)]
/// struct Provider2;
/// #[derive(PartialEq, Debug)]
/// struct Provider3;
///
/// // Combine them into one:
/// let forking1 = icu_provider_adapters::make_forking_provider!(
///     ForkByMarkerProvider::new,
///     [Provider1, Provider2, Provider3,]
/// );
///
/// // This is equivalent to:
/// let forking2 = ForkByMarkerProvider::new(
///     Provider1,
///     ForkByMarkerProvider::new(Provider2, Provider3),
/// );
///
/// assert_eq!(forking1, forking2);
/// ```
#[macro_export]
macro_rules! make_forking_provider {
    // Base case:
    ($combo_p:path, [ $a:expr, $b:expr, ]) => {
        $combo_p($a, $b)
    };
    // General list:
    ($combo_p:path, [ $a:expr, $b:expr, $($c:expr),+, ]) => {
        $combo_p($a, $crate::make_forking_provider!($combo_p, [ $b, $($c),+, ]))
    };
}

#[cfg(test)]
mod test {
    struct Provider1;
    struct Provider2;
    struct Provider3;

    #[test]
    fn test_make_forking_provider() {
        make_forking_provider!(
            crate::fork::ForkByMarkerProvider::new,
            [Provider1, Provider2, Provider3,]
        );
    }
}
