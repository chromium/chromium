'use strict';
const test_desc = 'disconnect() called before FUNCTION_NAME. ' +
    'Reject with NetworkError.';
let device, characteristic, fake_peripheral;

function createDOMException(func) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTCharacteristic': ` +
      `GATT Server is disconnected. Cannot retrieve descriptors. (Re)connect ` +
      `first with \`device.gatt.connect\`.`,
      'NetworkError');
}

bluetooth_test(() => getMeasurementIntervalCharacteristic()
    .then(_ => ({device, characteristic, fake_peripheral} = _))
    .then(() => {
      device.gatt.disconnect();
      return assert_promise_rejects_with_message(
        characteristic.CALLS([
          getDescriptor(user_description.name) |
          getDescriptors(user_description.name)[UUID] |
          getDescriptors()
        ]),
        createDOMException('FUNCTION_NAME'));
    }),
    test_desc);
