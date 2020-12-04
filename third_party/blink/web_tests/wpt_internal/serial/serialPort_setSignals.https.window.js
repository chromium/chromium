// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await promise_rejects_dom(t, 'InvalidStateError', port.setSignals({}));
}, 'setSignals() rejects if the port is not open');

serial_test(async (t, fake) => {
  const {port, fakePort} = await getFakeSerialPort(fake);
  await port.open({baudRate: 9600});

  let expectedSignals = {
    dataTerminalReady: true,
    requestToSend: false,
    break: false
  };
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'initial');

  await promise_rejects_js(t, TypeError, port.setSignals());
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'no-op');

  await promise_rejects_js(t, TypeError, port.setSignals({}));
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'no-op');

  await port.setSignals({dataTerminalReady: false});
  expectedSignals.dataTerminalReady = false;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'clear DTR');

  await port.setSignals({requestToSend: true});
  expectedSignals.requestToSend = true;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'set RTS');

  await port.setSignals({break: true});
  expectedSignals.break = true;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'set BRK');

  await port.setSignals(
      {dataTerminalReady: true, requestToSend: false, break: false});
  expectedSignals.dataTerminalReady = true;
  expectedSignals.requestToSend = false;
  expectedSignals.break = false;
  assert_object_equals(fakePort.outputSignals, expectedSignals, 'invert');
}, 'setSignals() modifies the state of the port');
