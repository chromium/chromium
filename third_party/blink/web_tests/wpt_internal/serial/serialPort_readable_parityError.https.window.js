// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

// ParityError is not (as of 2020/03/23) a valid DOMException, so cannot use
// promise_rejects_dom for it.
async function promise_rejects_with_parity_error(t, promise) {
  return promise
      .then(() => {
        assert_false('Should have rejected');
      })
      .catch(e => {
        assert_equals(e.constructor, DOMException);
        assert_equals(e.name, 'ParityError');
      });
}

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  // Select a buffer size smaller than the amount of data transferred.
  await port.open({baudRate: 9600, bufferSize: 64});

  let readable = port.readable;
  let reader = readable.getReader();

  await fakePort.writable();
  const data = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
  fakePort.write(data);
  fakePort.simulateParityError();

  let {value, done} = await reader.read();
  assert_false(done);
  compareArrays(data, value);

  await promise_rejects_with_parity_error(t, reader.read());
  assert_not_equals(port.readable, readable);

  readable = port.readable;
  assert_true(readable instanceof ReadableStream);
  reader = port.readable.getReader();

  await fakePort.writable();
  fakePort.write(data);

  ({value, done} = await reader.read());
  assert_false(done);
  compareArrays(data, value);
  reader.releaseLock();

  await port.close();
  assert_equals(port.readable, null);
}, 'Parity error closes readable and replaces it with a new stream');
