'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => {
      return gatt.getPrimaryService('heart_rate')
        .then(heart_rate_service => {
          let promise = assert_promise_rejects_with_message(
            heart_rate_service.CALLS([
              getCharacteristic('heart_rate_measurement')|
              getCharacteristics()|
              getCharacteristics('heart_rate_measurement')[UUID]
            ]),
            new DOMException(
              'GATT Server is disconnected. Cannot retrieve characteristics. ' +
              '(Re)connect first with `device.gatt.connect`.',
              'NetworkError'));
          gatt.disconnect();
          return promise;
        });
    });
}, 'disconnect() called during FUNCTION_NAME. Reject with NetworkError.');
