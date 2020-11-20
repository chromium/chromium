// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const eventWatcher =
      new EventWatcher(t, navigator.serial, ['connect', 'disconnect']);

  // Wait for getPorts() to resolve in order to ensure that the Mojo client
  // interface has been configured.
  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);

  fake.addPort();
  const event1 = await eventWatcher.wait_for(['connect']);
  assert_true(event1 instanceof Event);
  assert_true(event1.target instanceof SerialPort);

  fake.addPort();
  const event2 = await eventWatcher.wait_for(['connect']);
  assert_true(event2 instanceof Event);
  assert_true(event2.target instanceof SerialPort);
  assert_not_equals(event1.target, event2.target);

  ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 2);
  assert_in_array(event1.target, ports);
  assert_in_array(event2.target, ports);
}, 'A "connect" event is fired when ports are added.');
