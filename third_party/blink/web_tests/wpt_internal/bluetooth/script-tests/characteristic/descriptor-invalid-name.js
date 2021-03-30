'use strict';
bluetooth_test(() => {
  let promise;
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      return assert_promise_rejects_with_message(
        characteristic.CALLS([
          getDescriptor('invalid-name')|
          getDescriptors('invalid-name')]),
          new DOMException(
              'Failed to execute \'FUNCTION_NAME\' on ' +
                  '\'BluetoothRemoteGATTCharacteristic\': Invalid Descriptor name: ' +
                  '\'invalid-name\'. ' +
                  'It must be a valid UUID alias (e.g. 0x1234), ' +
                  'UUID (lowercase hex characters e.g. ' +
                  '\'00001234-0000-1000-8000-00805f9b34fb\'), ' +
                  'or recognized standard name from ' +
                  'https://www.bluetooth.com/specifications/gatt/descriptors' +
                  ' e.g. \'gatt.characteristic_presentation_format\'.',
              'TypeError'));

    })
}, 'Test to ensure FUNCTION_NAME throws when called with an invalid name.');
