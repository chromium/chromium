'use strict';
bluetooth_test(
    () => {
      let val = new Uint8Array([1]);
      return setBluetoothFakeAdapter('FailingGATTOperationsAdapter')
          .then(
              () => requestDeviceWithTrustedClick(
                  {filters: [{services: [errorUUID(0xA0)]}]}))
          .then(device => device.gatt.connect())
          .then(gattServer => {
            return gattServer.getPrimaryService(errorUUID(0xA0))
                .then(service => service.getCharacteristic(errorUUID(0xA1)))
                .then(error_characteristic => {
                  let promise = assert_promise_rejects_with_message(
                      error_characteristic.CALLS(
                          [getDescriptor(user_description.name) |
                           getDescriptors(user_description.name)[UUID] |
                           getDescriptors()]),
                      new DOMException(
                          'GATT Server is disconnected. Cannot retrieve descriptors. ' +
                              '(Re)connect first with `device.gatt.connect`.',
                          'NetworkError'));
                  gattServer.disconnect();
                  return promise;
                });
          });
    },
    'disconnect() called during a FUNCTION_NAME call that fails. ' +
        'Reject with NetworkError.');
