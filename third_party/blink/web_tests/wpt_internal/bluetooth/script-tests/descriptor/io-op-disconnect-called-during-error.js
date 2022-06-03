'use strict';
const test_desc = 'disconnect() called during a FUNCTION_NAME call that ' +
    'fails. Reject with NetworkError.';
const value = new Uint8Array([1]);
const expected = new DOMException(
    'GATT Server is disconnected. Cannot perform GATT operations. ' +
    '(Re)connect first with `device.gatt.connect`.',
    'NetworkError');
let fake_descriptor, device, descriptor;

bluetooth_test(() => getUserDescriptionDescriptor()
    .then(_ => ({fake_descriptor, device, descriptor} = _))
    .then(() => {
      switch ('FUNCTION_NAME') {
        case 'readValue':
          return fake_descriptor.setNextReadResponse(GATT_INVALID_HANDLE, null);
        case 'writeValue':
          return fake_descriptor.setNextWriteResponse(GATT_INVALID_HANDLE);
        default:
      }
    })
    .then(() => {
        let promise = assert_promise_rejects_with_message(
            descriptor.CALLS([readValue()|writeValue(value)]), expected);
        device.gatt.disconnect();
        return promise;
    }),
    test_desc);
