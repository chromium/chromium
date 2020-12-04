// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  assert_equals(port.readable, null);

  await port.open({baudRate: 9600});
  const readable = port.readable;
  assert_true(readable instanceof ReadableStream);

  await port.close();
  assert_equals(port.readable, null);

  const reader = readable.getReader();
  const {value, done} = await reader.read();
  assert_true(done);
  assert_equals(value, undefined);
}, 'SerialPort.readable is set by open() and closes on port close');
