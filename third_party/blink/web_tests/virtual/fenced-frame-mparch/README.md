# Fenced Frames

This directory contains fenced frame test expectations. The tests are run with
the flags:
```
--enable-features=
    FencedFrames:implementation_type/mparch,
    PrivacySandboxAdsAPIsOverride,
    NoncedPartitionedCookies,
    Fledge,
    InterestGroupStorage,
    AdInterestGroupAPI,
    AllowURNsInIframes,
    BiddingAndScoringDebugReportingAPI,
--enable-blink-features=
    FencedFramesAPIChanges
```


See [crbug.com/1123606](crbug.com/1123606) and
[crbug.com/1347953](crbug.com/1347953).
