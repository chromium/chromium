# WebHID Testing

Automated testing for the [WebHID API] uses [MojoJS] to override the
implementation of the [HidService] Mojo interface with a testing version in
`/resources/chromium/fake-hid.js`.

Most of these tests can be upstreamed to the Web Platform Tests repository by
creating an abstraction between the test cases and this Chromium-specific test
API similar to what has been done for [WebUSB] and [Web Serial].

Tests with the "-manual" suffix do not use the test-only interface and expect a
real hardware device to be connected. The specific characteristics of the device
are described in each test.

[HidService]: ../../../public/mojom/hid/hid.mojom
[MojoJS]: ../../../../../docs/testing/web_platform_tests.md#mojojs
[WebHID API]: https://wicg.github.io/webhid
[WebUSB]: ../../external/wpt/webusb
[Web Serial]: ../../external/wpt/serial
