// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await promise_rejects_dom(t, 'InvalidStateError', port.close());
}, 'A SerialPort cannot be closed if it was never opened.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await port.open({baudRate: 9600});
  await port.close();
  await promise_rejects_dom(t, 'InvalidStateError', port.close());
}, 'A SerialPort cannot be closed if it is already closed.');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);

  await port.open({baudRate: 9600});
  const closePromise = port.close();
  await promise_rejects_dom(t, 'InvalidStateError', port.close());
  await closePromise;
}, 'A SerialPort cannot be closed if it is being closed.');
