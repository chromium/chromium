'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]
    }))
    .then(device => device.gatt.connect())
    .then(gatt => {
      return setBluetoothFakeAdapter('EmptyAdapter')
        .then(() => assert_promise_rejects_with_message(
          gatt.CALLS([
            getPrimaryService('heart_rate')|
            getPrimaryServices()|
            getPrimaryServices('heart_rate')[UUID]
          ]),
          new DOMException('Bluetooth Device is no longer in range.',
                           'NetworkError'),
          'Device went out of range.'));
    });
}, 'Device goes out of range. Reject with NetworkError.');
