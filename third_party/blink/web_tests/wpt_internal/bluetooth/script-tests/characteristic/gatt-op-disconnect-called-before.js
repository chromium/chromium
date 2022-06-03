'use strict';
const test_desc = 'disconnect() called before FUNCTION_NAME. ' +
    'Reject with NetworkError.';
const value = new Uint8Array([1]);
let device, characteristic, fake_peripheral;

function createDOMException(func) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTCharacteristic': ` +
      `GATT Server is disconnected. Cannot perform GATT operations. ` +
      `(Re)connect first with \`device.gatt.connect\`.`,
      'NetworkError');
}

bluetooth_test(() => getMeasurementIntervalCharacteristic()
    .then(_ => ({device, characteristic, fake_peripheral} = _))
    .then(() => {
      device.gatt.disconnect();
      return assert_promise_rejects_with_message(
        characteristic.CALLS([
          readValue()|
          writeValue(value)|
          writeValueWithResponse(value)|
          writeValueWithoutResponse(value)|
          startNotifications()|
          stopNotifications()
        ]),
        createDOMException('FUNCTION_NAME'));
    }),
    test_desc);
