'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('generic_access'))
    .then(service => {
      return setBluetoothFakeAdapter('EmptyAdapter')
        .then(() => assert_promise_rejects_with_message(
          service.CALLS([
            getCharacteristic('gap.device_name')|
            getCharacteristics()|
            getCharacteristics('gap.device_name')[UUID]
          ]),
          new DOMException('Bluetooth Device is no longer in range.',
                           'NetworkError'),
          'Device went out of range.'));
    });
}, 'FUNCTION_NAME called after device goes out of range. Reject with ' +
   'NetworkError.');
