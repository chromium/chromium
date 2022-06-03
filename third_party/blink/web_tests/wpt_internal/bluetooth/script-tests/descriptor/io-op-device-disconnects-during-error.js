'use strict';
bluetooth_test(
    () => {
      let val = new Uint8Array([1]);
      return setBluetoothFakeAdapter('FailingGATTOperationsAdapter')
          .then(() => requestDeviceWithTrustedClick({
                  filters: [{services: [errorUUID(0xA0)]}],
                  optionalServices: [request_disconnection_service_uuid]
                }))
          .then(device => device.gatt.connect())
          .then(gattServer => {
            let error_characteristic;
            return gattServer.getPrimaryService(errorUUID(0xA0))
                .then(es => es.getCharacteristic(errorUUID(0xA1)))
                .then(
                    characteristic => characteristic.getDescriptor(
                        'gatt.characteristic_user_description'))
                .then(d => user_description = d)
                .then(() => get_request_disconnection(gattServer))
                .then(requestDisconnection => {
                  requestDisconnection();
                  return assert_promise_rejects_with_message(
                      user_description.CALLS([readValue()|writeValue(val)]),
                      new DOMException(
                        'GATT Server is disconnected. Cannot perform GATT operations. ' +
                          '(Re)connect first with `device.gatt.connect`.',
                          'NetworkError'));
                });
          });
    },
    'Device disconnects during a FUNCTION_NAME call that fails. ' +
        'Reject with NetworkError.');
