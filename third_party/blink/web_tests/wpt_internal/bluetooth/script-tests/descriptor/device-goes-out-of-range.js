'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
      .then(
          () => requestDeviceWithTrustedClick(
              {filters: [{services: ['health_thermometer']}]}))
      .then(device => device.gatt.connect())
      .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
      .then(service => service.getCharacteristic('measurement_interval'))
      .then(
          characteristic => characteristic.getDescriptor(user_description.name))
      .then(descriptor => {
        return setBluetoothFakeAdapter('EmptyAdapter')
            .then(
                () => assert_promise_rejects_with_message(
                    descriptor.CALLS([readValue()]),
                    new DOMException(
                        'Bluetooth Device is no longer in range.',
                        'NetworkError'),
                    'Device went out of range.'));
      });
}, 'Device goes out of range. Reject with NetworkError.');
