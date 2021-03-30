// TODO(509038): This is a temporary file that will allow bluetooth-test.js
// and bluetooth-fake-devices.js to be migrated into the wpt/ directory while
// allowing the tests that still use FakeAdapter to continue working until they
// are converted to the new testing API. FakeAdapter is the test API that
// Bluetooth test allows user agents to create fake devices that have certain
// characteristics and behave in a certain way. As a result of this, these
// devices do not allow for a more granular control over the devices. Therefore,
// we are currently moving bluetooth tests away from FakeAdapter to a new test
// API that will allow for more control over the Bluetooth devices. This
// migration is described in the following design document:
// https://docs.google.com/document/d/1Nhv_oVDCodd1pEH_jj9k8gF4rPGb_84VYaZ9IG8M_WY/edit#heading=h.7nki9mck5t64
'use strict';

function assert_testRunner() {
  assert_true(
      window.testRunner instanceof Object,
      'window.testRunner is required for this test, it will not work manually.');
}

function setBluetoothManualChooser(enable) {
  assert_testRunner();
  testRunner.setBluetoothManualChooser(enable);
}

// Calls testRunner.getBluetoothManualChooserEvents() until it's returned
// |expected_count| events. Or just once if |expected_count| is undefined.
function getBluetoothManualChooserEvents(expected_count) {
  assert_testRunner();
  return new Promise((resolve, reject) => {
    let events = [];
    let accumulate_events = new_events => {
      events.push(...new_events);
      if (events.length >= expected_count) {
        resolve(events);
      } else {
        testRunner.getBluetoothManualChooserEvents(accumulate_events);
      }
    };
    testRunner.getBluetoothManualChooserEvents(accumulate_events);
  });
}

function sendBluetoothManualChooserEvent(event, argument) {
  assert_testRunner();
  testRunner.sendBluetoothManualChooserEvent(event, argument);
}

function setBluetoothFakeAdapter(adapter_name) {
  assert_testRunner();
  return new Promise(resolve => {
    testRunner.setBluetoothFakeAdapter(adapter_name, resolve);
  });
}

// Parses add-device(name)=id lines in
// testRunner.getBluetoothManualChooserEvents() output, and exposes the name->id
// mapping.
class AddDeviceEventSet {
  constructor() {
    this._idsByName = new Map();
    this._addDeviceRegex = /^add-device\(([^)]+)\)=(.+)$/;
  }
  assert_add_device_event(event, description) {
    let match = this._addDeviceRegex.exec(event);
    assert_true(!!match, event + ' isn\'t an add-device event: ' + description);
    this._idsByName.set(match[1], match[2]);
  }
  has(name) {
    return this._idsByName.has(name);
  }
  get(name) {
    return this._idsByName.get(name);
  }
}

/**
 * The following tests are used in the legacy BluetoothFakeAdapter test API
 * used in private Chromium web tests. The tests using the new FakeBluetooth
 * test API have the ability to set the next response of an operation.
 *
 * TODO(569709): Remove this variable once all tests are using the FakeBluetooth
 * test API.
 */
var gatt_errors_tests = [
  {
    testName: 'GATT Error: Unknown.',
    uuid: errorUUID(0xA1),
    error: new DOMException('GATT Error Unknown.', 'NotSupportedError')
  },
  {
    testName: 'GATT Error: Failed.',
    uuid: errorUUID(0xA2),
    error: new DOMException(
        'GATT operation failed for unknown reason.', 'NotSupportedError')
  },
  {
    testName: 'GATT Error: In Progress.',
    uuid: errorUUID(0xA3),
    error:
        new DOMException('GATT operation already in progress.', 'NetworkError')
  },
  {
    testName: 'GATT Error: Invalid Length.',
    uuid: errorUUID(0xA4),
    error: new DOMException(
        'GATT Error: invalid attribute length.', 'InvalidModificationError')
  },
  {
    testName: 'GATT Error: Not Permitted.',
    uuid: errorUUID(0xA5),
    error:
        new DOMException('GATT operation not permitted.', 'NotSupportedError')
  },
  {
    testName: 'GATT Error: Not Authorized.',
    uuid: errorUUID(0xA6),
    error: new DOMException('GATT operation not authorized.', 'SecurityError')
  },
  {
    testName: 'GATT Error: Not Paired.',
    uuid: errorUUID(0xA7),
    // TODO(ortuno): Change to InsufficientAuthenticationError or similar
    // once https://github.com/WebBluetoothCG/web-bluetooth/issues/137 is
    // resolved.
    error: new DOMException('GATT Error: Not paired.', 'NetworkError')
  },
  {
    testName: 'GATT Error: Not Supported.',
    uuid: errorUUID(0xA8),
    error: new DOMException('GATT Error: Not supported.', 'NotSupportedError')
  }
];

/**
 * errorUUID(alias) returns a UUID with the top 32 bits of
 * '00000000-97e5-4cd7-b9f1-f5a427670c59' replaced with the bits of |alias|.
 * For example, errorUUID(0xDEADBEEF) returns
 * 'deadbeef-97e5-4cd7-b9f1-f5a427670c59'. The bottom 96 bits of error UUIDs
 * were generated as a type 4 (random) UUID.
 *
 * This method is only used in private Chromium web tests that are using the
 * legacy BluetoothFakeAdapter test API.
 *
 * TODO(569709): Remove this variable once all tests are using the FakeBluetooth
 * test API.
 */
function errorUUID(uuidAlias) {
  // Make the number positive.
  uuidAlias >>>= 0;
  // Append the alias as a hex number.
  var strAlias = '0000000' + uuidAlias.toString(16);
  // Get last 8 digits of strAlias.
  strAlias = strAlias.substr(-8);
  // Append Base Error UUID
  return strAlias + '-97e5-4cd7-b9f1-f5a427670c59';
}

/**
 * Returns a function that when called returns a promise that resolves when
 * the device has disconnected. Example:
 * device.gatt.connect()
 *   .then(gatt => get_request_disconnection(gatt))
 *   .then(requestDisconnection => requestDisconnection())
 *   .then(() => // device is now disconnected)
 *
 * This method is only used in private Chromium web tests that are using the
 * legacy BluetoothFakeAdapter test API.
 *
 * TODO(569709): Remove this variable once all tests are using the FakeBluetooth
 * test API.
 */
function get_request_disconnection(gattServer) {
  return gattServer.getPrimaryService(request_disconnection_service_uuid)
      .then(
          service => service.getCharacteristic(
              request_disconnection_characteristic_uuid))
      .then(characteristic => {
        return () => assert_promise_rejects_with_message(
                   characteristic.writeValue(new Uint8Array([0])),
                   new DOMException(
                       'GATT Server is disconnected. Cannot perform GATT operations. ' +
                           '(Re)connect first with `device.gatt.connect`.',
                       'NetworkError'));
      });
}
