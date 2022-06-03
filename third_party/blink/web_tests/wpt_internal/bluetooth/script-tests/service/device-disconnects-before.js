'use strict';
function createDOMException(func) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTService': ` +
      `GATT Server is disconnected. Cannot retrieve characteristics. ` +
      `(Re)connect first with \`device.gatt.connect\`.`,
      'NetworkError');
}

bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}],
      optionalServices: [request_disconnection_service_uuid]
    }))
    .then(device => device.gatt.connect())
    .then(gatt => {
      let heart_rate_service;
      return gatt.getPrimaryService('heart_rate')
        .then(hrs => heart_rate_service = hrs)
        .then(() => get_request_disconnection(gatt))
        .then(requestDisconnection => requestDisconnection())
        .then(() => assert_promise_rejects_with_message(
          heart_rate_service.CALLS([
            getCharacteristic('heart_rate_measurement')|
            getCharacteristics()|
            getCharacteristics('heart_rate_measurement')[UUID]
          ]),
          createDOMException('FUNCTION_NAME')));
    });
}, 'Device disconnects before FUNCTION_NAME call. Reject with NetworkError.');
