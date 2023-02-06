# Shared Storage

The tests are run with the flag:
```
--enable-features=
  SharedStorageAPI,
  FencedFrames:implementation\_type/mparch,
  PrivacySandboxAdsAPIsOverride,
  FencedFramesAPIChanges
```

You can run these tests by targeting the following directory:
`virtual/shared-storage-fenced-frame-mparch/wpt_internal/shared_storage.

The `FencedFramesAPIChanges` feature is enabled for user to specify the behavior
of `selectURL` by changing the boolean field `resolveToConfig` of
`SharedStorageRunOperationMethodOptions`:
1. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0}});`
resolves to an urn::uuid.
2. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0},
resolveToConfig: false});` resolves to an urn::uuid.
3. `sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0},
resolveToConfig: true});` resolves to a fenced frame config object.

See [crbug.com/1218540](crbug.com/1218540) and
[crbug.com/1347953](crbug.com/1347953).
