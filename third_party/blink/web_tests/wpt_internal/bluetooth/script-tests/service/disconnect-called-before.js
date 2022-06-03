'use strict';
function createDOMException(func) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTService': ` +
      `GATT Server is disconnected. Cannot retrieve characteristics. ` +
      `(Re)connect first with \`device.gatt.connect\`.`,
      'NetworkError');
}

bluetooth_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => {
      return gatt.getPrimaryService('heart_rate')
        .then(heart_rate_service => {
          gatt.disconnect();
          return assert_promise_rejects_with_message(
            heart_rate_service.CALLS([
              getCharacteristic('heart_rate_measurement')|
              getCharacteristics()|
              getCharacteristics('heart_rate_measurement')[UUID]
            ]),
            createDOMException('FUNCTION_NAME'));
        });
    });
}, 'disconnect() called before FUNCTION_NAME. Reject with NetworkError.');
