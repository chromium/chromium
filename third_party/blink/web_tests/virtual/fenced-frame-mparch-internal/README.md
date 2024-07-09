# Fenced Frames Internal

This directory contains `wpt_internal/fenced_frame/` test expectations. The
tests are run with the same flags as the external-facing 'fenced-frame-mparch'
suite:
```
--enable-features=
    FencedFrames:implementation_type/mparch,
    PrivacySandboxAdsAPIsOverride,
    SharedStorageAPI,
    NoncedPartitionedCookies,
    Fledge,
    InterestGroupStorage,
    AdInterestGroupAPI,
    AllowURNsInIframes,
    BiddingAndScoringDebugReportingAPI,
--enable-blink-features=
    FencedFramesAPIChanges
```
as well as these internal-specific flags:
```
--private-aggregation-developer-mode
```

This test suite must only be limited to the `wpt_internal/` tests since it
exposes debug behavior that is not implemented in WebDriver yet. This suite
can be removed after this bug is fixed: https://crbug.com/350526049.