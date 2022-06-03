'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('ServicesDiscoveredAfterReconnectionAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => {
      let promise = assert_promise_rejects_with_message(
        gatt.CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]
        ]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
      gatt.disconnect();
      return gatt.connect().then(() => promise);
    });
}, 'disconnect() and connect() called during a FUNCTION_NAME call that ' +
   'succeeds. Reject with NetworkError.');
