'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingDuringServiceRetrievalAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]
    }))
    .then(device => device.gatt.connect())
    .then(gatt => assert_promise_rejects_with_message(
      gatt.CALLS([
        getPrimaryService('heart_rate') |
        getPrimaryServices() |
        getPrimaryServices('heart_rate')[UUID]]),
      new DOMException(
        'GATT Server is disconnected. Cannot retrieve services. ' +
        '(Re)connect first with `device.gatt.connect`.',
        'NetworkError')));
}, 'Device disconnects during FUNCTION_NAME call. Reject with NetworkError.');
