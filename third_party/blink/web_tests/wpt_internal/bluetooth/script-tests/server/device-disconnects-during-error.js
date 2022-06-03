'use strict';
bluetooth_test(t => {
  return setBluetoothFakeAdapter('DisconnectingHeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}],
      optionalServices: [request_disconnection_service_uuid,
                         'battery_service']
    }))
    .then(device => {
      return device.gatt.connect()
        .then(gatt => get_request_disconnection(gatt))
        .then(requestDisconnection => {
          requestDisconnection();
          return assert_promise_rejects_with_message(
            // getPrimaryServices() can't fail because we request access to the
            // disconnection service, which we need for this test.
            // TODO(crbug.com/719816): Add a call to getPrimaryServices() when
            // the disconnection service is no longer needed.
            device.gatt.CALLS([
              getPrimaryService('battery_service')|
              getPrimaryServices('battery_service')[UUID]
            ]),
            new DOMException('GATT Server is disconnected. ' +
                             'Cannot retrieve services. ' +
                             '(Re)connect first with `device.gatt.connect`.',
                             'NetworkError'));
        });
    });
}, 'Device disconnects during a FUNCTION_NAME call that fails. Reject ' +
   'with NetworkError.');
