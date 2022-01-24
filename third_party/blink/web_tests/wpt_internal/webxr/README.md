# WebXR Internal Tests
In keeping with the requirements for the wpt\_internal tests, these tests
validate behavior that is not yet in a stable version of the spec (e.g. AR),
test Chrome specific constants, or test things specific to the blink/mojom
implementations.

In order to reference xr-internal-device-mocking.js (or other additional files
that may be required for an internal test), the additionalChromiumResources
variable/array should be set before including webxr\_util.js, as webxr\_util.js
begins loading resources immediately.

To port a test from wpt\_internal to external/wpt, the "/webxr/resources/..."
paths will need to be converted to "resources/..." paths, and any reliance on
internal APIs (i.e. any usage of additionalChromiumResources) should be removed.
Those should be the only changes required other than those required by wpt lint.

For more details, please reference the wpt_internal [README] [WPT Readme].

[WPT Readme]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/web_tests/wpt_internal/README.md
