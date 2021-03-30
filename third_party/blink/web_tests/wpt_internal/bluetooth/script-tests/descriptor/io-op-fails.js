'use strict';
bluetooth_test(
    () => {
        let val = new Uint8Array([1]);
        return setBluetoothFakeAdapter('FailingGATTOperationsAdapter')
            .then(
                () => requestDeviceWithTrustedClick(
                    {filters: [{services: [errorUUID(0xA0)]}]}))
            .then(device => device.gatt.connect())
            .then(gattServer => gattServer.getPrimaryService(errorUUID(0xA0)))
            .then(
                service => {
                    service.getCharacteristic(errorUUID(0xA1))
                        .then(characteristic => {
                          let tests = Promise.resolve();
                          gatt_errors_tests.forEach(testSpec => {
                            tests =
                                tests
                                    .then(
                                        () => characteristic.getDescriptor(
                                            testSpec.uuid))
                                    .then(
                                        descriptor =>
                                            assert_promise_rejects_with_message(
                                                descriptor.CALLS([readValue()|writeValue(val)]),
                                                testSpec.error,
                                                testSpec.testName));
                          });
                          return tests;
                        })})},
    'FUNCTION_NAME fails. Should reject with appropriate error.');
