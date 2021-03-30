'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingDuringServiceRetrievalAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['battery_service']}
    ))
    .then(device => device.gatt.connect())
    .then(gatt => {
      let disconnected = eventPromise(gatt.device, 'gattserverdisconnected');
      let promise = assert_promise_rejects_with_message(
        // TODO(crbug.com/719816): Add a call to getPrimaryServices() when
        // the adapter doesn't include services.
        gatt.CALLS([
          getPrimaryService('battery_service')|
          getPrimaryServices('battery_service')[UUID]]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
      return disconnected.then(() => gatt.connect()).then(() => promise);
    });
}, 'Device disconnects and we reconnect during a FUNCTION_NAME call ' +
   'that fails. Reject with NetworkError.');
