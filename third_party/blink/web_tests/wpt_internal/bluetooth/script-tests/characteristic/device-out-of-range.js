// TODO(672127) Use this test case to test the rest of characteristic functions.
'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      return setBluetoothFakeAdapter('EmptyAdapter')
        .then(() => assert_promise_rejects_with_message(
          characteristic.CALLS([
            getDescriptor(user_description.name)|
            getDescriptors(user_description.name)[UUID]|
            getDescriptors()]),
          new DOMException('Bluetooth Device is no longer in range.',
                           'NetworkError'),
        'Device went out of range.'));
    });
}, 'Device goes out of range. Reject with NetworkError.');
