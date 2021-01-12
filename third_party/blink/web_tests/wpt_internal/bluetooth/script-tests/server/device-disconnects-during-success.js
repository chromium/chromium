'use strict';
bluetooth_test(t => {
  return setBluetoothFakeAdapter('DisconnectingHeartRateAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}],
      optionalServices: [request_disconnection_service_uuid]
    }))
    .then(device => {
      return device.gatt.connect()
        .then(gatt => get_request_disconnection(gatt))
        .then(requestDisconnection => {
          requestDisconnection();
          return assert_promise_rejects_with_message(
            device.gatt.CALLS([
              getPrimaryService('heart_rate')|
              getPrimaryServices()|
              getPrimaryServices('heart_rate')[UUID]
            ]),
            new DOMException('GATT Server is disconnected. ' +
                             'Cannot retrieve services. ' +
                             '(Re)connect first with `device.gatt.connect`.',
                             'NetworkError'));
        });
    });
}, 'Device disconnects during a FUNCTION_NAME call that succeeds. Reject ' +
   'with NetworkError.');
