# Web Bluetooth API Web Tests

This directory contains Chromium only Web Bluetooth API web tests.

The [Web Bluetooth specification] is implemented by the Web Bluetooth Service
in `//content/browser/bluetooth`. See the [Web Bluetooth Service README] for
more details.

[Web Bluetooth specification]: https://WebBluetoothCG.github.io/web-bluetooth
[Web Bluetooth Service README]: ../../../../content/browser/bluetooth/README.md

# Test Refactoring Effort

The tests in this directory are currently undergoing a refactoring in order to
migrate them into `//third_party/blink/web_tests/external/wpt/bluetooth` so that
other implementers are able to reuse the tests for their own implementation.
See the [web-platform-tests Bluetooth README] for more details.

These tests are currently using the legacy [BluetoothFakeAdapter] test
infrastructure which defines several mock Bluetooth devices with specific
characteristics and behaviors. This test infrastructure lacks granular
control over the test devices, which ends up requiring workarounds and hacks
resulting in flaky tests. The new [FakeBluetooth] test infrastructure
produces dynamic test devices that can be controlled during a test. For more
details on the refactoring effort, see the [Web Bluetooth Testing] design
document.

TODO(https://crbug.com/509038): Update this document when the remaining tests
have been submitted to W3C Web Platform Tests.

There are still a few APIs missing from [FakeBluetooth] that need to be
implemented in order to refactor the remaining tests in this directory. A list
of the APIs needed by these tests can be found in the [Web Bluetooth Test API
Dependencies] spreadsheet.

TODO(https://crbug.com/569709) : Update this document when the remaining
[FakeBluetooth] APIs are implemented.

[BluetoothFakeAdapter]:
../../../../content/shell/browser/web_test/web_test_bluetooth_adapter_provider.h
[FakeBluetooth]:
../../../../device/bluetooth/emulation/fake_bluetooth.h
[Web Bluetooth Testing]:
https://docs.google.com/document/d/1Nhv_oVDCodd1pEH_jj9k8gF4rPGb_84VYaZ9IG8M_WY
[Web Bluetooth Test API Dependencies]:
https://docs.google.com/spreadsheets/d/1L4t6Um9lpoyv17rlm3EBXIP6bxEQG4klVCwF1SANHa4
[web-platform-tests Bluetooth README]: ../external/wpt/bluetooth/README.md

# Generated Tests

Several Web Bluetooth tests share common test logic. For these tests, the
`script-tests` directory contains templates that are used by the
`generate.py` script to create several tests from these templates. The
templates are JavaScript files that contain a `CALLS()` keyword with
functions delimited by a `|` character. A test will be created for each
function in the `CALLS()` by `generate.py`. Note that for each subdirectory
in `script-tests` there is a matching directory under `bluetooth`. The
generator will expand `CALLS` functions into the corresponding directory.

## Example

The `./script-tests/server/get-same-object.js` contains the following
code:

```js
gattServer.CALLS([
        getPrimaryService('heart_rate')|
        getPrimaryServices()|
        getPrimaryServices('heart_rate')[UUID]]),
```

The functions in `CALLS()` will be expanded to generate 3 test files prefixed
with `gen-`:

```
bluetooth/server/getPrimaryService/gen-get-same-object.html
bluetooth/server/getPrimaryServices/gen-get-same-object.html
bluetooth/server/getPrimaryServices/gen-get-same-object-with-uuid.html
```

## Generate Tests

To generate the tests in `script-tests`, run the following command from the
source root:

```sh
$ python third_party/blink/web_tests/wpt_internal/bluetooth/generate.py
```

To check that generated tests are correct and that there are no obsolete tests,
or tests for which a template does not exist anymore, run:

```sh
$ python third_party/blink/web_tests/wpt_internal/bluetooth/generate_test.py
```

More details can be found in `generate.py` and `generate_test.py`.

# Resources and Documentation

Mailing list: web-bluetooth@chromium.org

Bug tracker: [Blink>Bluetooth]

* [Web Bluetooth specification]
* [Web Bluetooth Service README]
* [Web Bluetooth Testing]
* [Web Bluetooth Test API Dependencies]
* [web-platform-tests Bluetooth README]

[Blink>Bluetooth]: https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EBluetooth&can=2
