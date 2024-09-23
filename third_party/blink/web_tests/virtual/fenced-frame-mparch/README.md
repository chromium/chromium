# Fenced Frames

This directory contains fenced frame test expectations. The tests are run with
the flags:
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

The `FencedFramesAPIChanges` feature is enabled for user to specify the behavior
of `selectURL` by changing the boolean field `resolveToConfig` of
`SharedStorageRunOperationMethodOptions`:
1. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0}});`
resolves to an urn::uuid.
2. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0},
resolveToConfig: false});` resolves to an urn::uuid.
3. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0},
resolveToConfig: true});` resolves to a fenced frame config object.


See [crbug.com/1123606](crbug.com/1123606) and
[crbug.com/1347953](crbug.com/1347953).
