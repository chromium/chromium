'use strict';
bluetooth_test(() => {
  let val = new Uint8Array([1]);
  let promise;
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(measurement_interval => {
      promise = assert_promise_rejects_with_message(
        measurement_interval.CALLS([
          readValue()|
          writeValue(val)|
          writeValueWithResponse(val)|
          writeValueWithoutResponse(val)|
          startNotifications()|
          stopNotifications()]),
        new DOMException(
          'GATT Server is disconnected. Cannot perform GATT operations. ' +
          '(Re)connect first with `device.gatt.connect`.',
          'NetworkError'));
      // Disconnect called to clear attributeInstanceMap and allow the
      // object to get garbage collected.
      measurement_interval.service.device.gatt.disconnect();
    })
    .then(garbageCollect)
    .then(() => promise);
}, 'Garbage collection ran during a FUNCTION_NAME call that succeeds. ' +
   'Should not crash.');
