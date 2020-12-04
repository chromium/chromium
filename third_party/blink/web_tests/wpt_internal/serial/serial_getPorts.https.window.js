// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

promise_test(async () => {
  let interceptor =
      new MojoInterfaceInterceptor(blink.mojom.SerialService.name);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);

  interceptor.stop();
}, 'getPorts() returns empty list if Mojo service connection fails');

serial_test(async (t, fake) => {
  fake.addPort();
  fake.addPort();

  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 2);
  assert_true(ports[0] instanceof SerialPort);
  assert_true(ports[1] instanceof SerialPort);
}, 'getPorts() returns the set of configured fake ports');

serial_test(async (t, fake) => {
  fake.addPort();

  let portsFirst = await navigator.serial.getPorts();
  assert_equals(portsFirst.length, 1, 'first call returns one port');
  assert_true(portsFirst[0] instanceof SerialPort);
  let portsSecond = await navigator.serial.getPorts();
  assert_equals(portsSecond.length, 1, 'second call returns one port');
  assert_true(portsSecond[0] instanceof SerialPort);
  assert_true(portsFirst[0] === portsSecond[0]);
}, 'getPorts() returns the same port objects every time');
