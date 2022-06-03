'use strict';
function createDOMException(func, uuid) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTCharacteristic': ` +
      `Characteristic with UUID ${uuid} is no longer valid. Remember to ` +
      `retrieve the characteristic again after reconnecting.`,
      'InvalidStateError');
}

bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['health_thermometer']}],
      optionalServices: [request_disconnection_service_uuid]}))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      let characteristics;
      return gattServer.getPrimaryService('health_thermometer')
        .then(service => service.CALLS([
          getCharacteristic('measurement_interval')|
          getCharacteristics()|
          getCharacteristics('measurement_interval')[UUID]]))
        .then(c => {
          // Convert to array if necessary.
          characteristics = [].concat(c);
          return get_request_disconnection(gattServer);
        })
        .then(requestDisconnection => requestDisconnection())
        .then(() => gattServer.connect())
        .then(() => characteristics);
    })
    .then(characteristics => {
      let promises = Promise.resolve();
      for (let characteristic of characteristics) {
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            characteristic.readValue(),
            createDOMException('readValue', characteristic.uuid)));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            characteristic.writeValue(new Uint8Array([1])),
            createDOMException('writeValue', characteristic.uuid)));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            characteristic.startNotifications(),
            createDOMException('startNotifications', characteristic.uuid)));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            characteristic.stopNotifications(),
            createDOMException('stopNotifications', characteristic.uuid)));
      }
      return promises;
    });
}, 'Calls on characteristics after device disconnects and we reconnect. ' +
   'Should reject with InvalidStateError.');
