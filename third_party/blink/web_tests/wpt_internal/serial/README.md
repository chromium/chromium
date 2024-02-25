# Web Serial Testing

Automated testing for the [Web Serial API] relies on a test-only interface which
must be provided by browsers under test. This is similar to [WebUSB] however
there is no separate specification of the API other than the tests themselves
and the Chromium implementation.

Tests in this suite include `resources/automation.js` to detect and load the
test API as needed and the Chromium implementation is provided by
`external/wpt/resources/chromium/fake-serial.js` using [MojoJS].

This directory contains Chromium-specific tests for the [Web Serial API]. Once Web Serial Test API spec is defined, tests should be upstreamed to the [equivalent Web Platform Tests directory] instead.

[equivalent Web Platform Tests directory]: ../../external/wpt/serial
[Web Serial API]: https://wicg.github.io/serial
[MojoJS]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/testing/web_platform_tests.md#mojojs
[WebUSB]: ../../external/wpt/webusb