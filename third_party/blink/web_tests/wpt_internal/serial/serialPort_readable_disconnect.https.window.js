// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  // Select a buffer size smaller than the amount of data transferred.
  await port.open({baudRate: 9600, bufferSize: 64});

  const reader = port.readable.getReader();

  await fakePort.writable();
  const data = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
  fakePort.write(data);
  fakePort.simulateDisconnectOnRead();

  const {value, done} = await reader.read();
  assert_false(done);
  compareArrays(data, value);

  await promise_rejects_dom(t, 'NetworkError', reader.read());
  assert_equals(port.readable, null);

  await port.close();
}, 'Disconnect error closes readable and sets it to null');
