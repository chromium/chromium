# requestStorageAccessForOrigin Tests
These tests are tentative. They are based on a proposed requestStorageAccessForOrigin extension to the Storage Access API which can be read about [in the explainer](https://github.com/mreichhoff/requestStorageAccessForOrigin).

Note that the spec is not yet defined, though very early [prose](https://github.com/mreichhoff/requestStorageAccessForOrigin#proposed-draft-spec-addition) and [bikeshed](https://github.com/mreichhoff/storage-access/commit/93ba79fdbb737f57a7ce757f994b2f8c53d2cd53) drafts are available.

## Running Tests

The WPTs run as [virtual tests](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/VirtualTestSuites). This is required as the proposed API is behind a feature flag.

```bash
# Build web tests
autoninja -C out/Default blink_tests

# Run a single test
third_party/blink/tools/run_web_tests.py -t Default third_party/blink/web_tests/wpt_internal/storage-access-api/requestStorageAccessForOrigin.sub.tentative.window.js
```

See the [web tests doc](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md#running-the-tests) for more details on using the test runner.
