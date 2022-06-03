'use strict';
let expected = new DOMException('Origin is not allowed to access the ' +
                                'service. Tip: Add the service UUID to ' +
                                '\'optionalServices\' in requestDevice() ' +
                                'options. https://goo.gl/HxfxSQ',
                                'SecurityError');
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DelayedServicesDiscoveryAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]
    }))
    .then(device => device.gatt.connect())
    .then(gatt => Promise.all([
      assert_promise_rejects_with_message(
        gatt.CALLS([
          getPrimaryService(generic_access.alias)|
          getPrimaryServices(generic_access.alias)[UUID]
        ]), expected),
      assert_promise_rejects_with_message(
        gatt.FUNCTION_NAME(generic_access.name), expected),
      assert_promise_rejects_with_message(
        gatt.FUNCTION_NAME(generic_access.uuid), expected)]));
}, 'Delayed service discovery, request for present service without ' +
   'permission. Reject with SecurityError.');
