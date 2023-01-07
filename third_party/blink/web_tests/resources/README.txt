The following files are used to run W3C testharness.js-based tests.
These files should not be modified locally, as they are manually
synced from LayoutTests/external/wpt/resources (https://crbug.com/685854),
which is automatically synced with W3C web-platform-tests.

* testdriver.js
* testharness.js
* testharness.css
* testdriver-actions.js

The following files are native to Blink and can be modified:

* testdriver-vendor.js    automation via Blink internal APIs
* testharnessreport.js    integration with Blink's test runner

See also:
https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_platform_tests.md

References:
* web-platform-tests  https://github.com/web-platform-tests/wpt
