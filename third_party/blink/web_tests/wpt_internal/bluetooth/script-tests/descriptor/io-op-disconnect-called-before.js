'use strict';
const test_desc = 'disconnect() called before FUNCTION_NAME. Reject with ' +
    'NetworkError.';
const value = new Uint8Array([1]);

function createDOMException(func) {
  return new DOMException(
      `Failed to execute '${func}' on 'BluetoothRemoteGATTDescriptor': ` +
      `GATT Server is disconnected. Cannot perform GATT operations. ` +
      `(Re)connect first with \`device.gatt.connect\`.`,
      'NetworkError');
}

bluetooth_test(() => getUserDescriptionDescriptor()
    .then(({device, descriptor}) => {
      device.gatt.disconnect();
      return assert_promise_rejects_with_message(
          descriptor.CALLS([readValue()|writeValue(value)]),
          createDOMException('FUNCTION_NAME'));
    }), test_desc);
