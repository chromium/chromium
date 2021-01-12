bluetooth_test(
    () => {
      let val = new Uint8Array([1]);
      return setBluetoothFakeAdapter(
                 'DisconnectingDuringFailureGATTOperationAdapter')
          .then(
              () => requestDeviceWithTrustedClick(
                  {filters: [{services: ['health_thermometer']}]}))
          .then(device => device.gatt.connect())
          .then(gatt => gatt.getPrimaryService('health_thermometer'))
          .then(service => service.getCharacteristic('measurement_interval'))
          .then(
              characteristic =>
                  characteristic.getDescriptor(user_description.name))
          .then(descriptor => {
            let disconnected = eventPromise(
                descriptor.characteristic.service.device,
                'gattserverdisconnected');
            let promise = assert_promise_rejects_with_message(
                descriptor.CALLS([readValue()|writeValue(val)]),
                new DOMException(
                  'GATT Server is disconnected. Cannot perform GATT operations. ' +
                    '(Re)connect first with `device.gatt.connect`.',
                    'NetworkError'));
            return disconnected
                .then(
                    () =>
                        descriptor.characteristic.service.device.gatt.connect())
                .then(() => promise);
          });
    },
    'Device reconnects during a FUNCTION_NAME call that fails. Reject ' +
        'with NetworkError.');
