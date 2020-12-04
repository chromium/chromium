// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

promise_test((t) => {
  return promise_rejects_dom(
      t, 'SecurityError', navigator.serial.requestPort());
}, 'requestPort() rejects without a user gesture');

promise_test(async (t) => {
  let interceptor =
      new MojoInterfaceInterceptor(blink.mojom.SerialService.name);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  await trustedClick();
  try {
    await promise_rejects_dom(
        t, 'NotFoundError', navigator.serial.requestPort());
  } finally {
    interceptor.stop();
  }
}, 'requestPort() rejects if Mojo service connection fails');

serial_test(async (t, fake) => {
  await trustedClick();
  return promise_rejects_dom(
      t, 'NotFoundError', navigator.serial.requestPort());
}, 'requestPort() rejects if no port has been selected');

serial_test(async (t, fake) => {
  let guid = fake.addPort();
  fake.setSelectedPort(guid);

  await trustedClick();
  let port = await navigator.serial.requestPort();
  assert_true(port instanceof SerialPort);
}, 'requestPort() returns the selected port');

serial_test(async (t, fake) => {
  let guid = fake.addPort();
  fake.setSelectedPort(guid);

  await trustedClick();
  let firstPort = await navigator.serial.requestPort();
  assert_true(firstPort instanceof SerialPort);
  let secondPort = await navigator.serial.requestPort();
  assert_true(secondPort instanceof SerialPort);
  assert_true(firstPort === secondPort);
}, 'requestPort() returns the same port object every time');

serial_test(async (t, fake) => {
  let guid = fake.addPort();
  fake.setSelectedPort(guid);

  await trustedClick();
  let port = await navigator.serial.requestPort({filters: []});
  assert_true(port instanceof SerialPort);
}, 'An empty list of filters is valid');

serial_test(async (t, fake) => {
  let guid = fake.addPort();
  fake.setSelectedPort(guid);

  await trustedClick();
  return promise_rejects_js(t, TypeError, navigator.serial.requestPort({
    filters: [{}],
  }));
}, 'An empty filter is not valid');

serial_test(async (t, fake) => {
  let guid = fake.addPort();
  fake.setSelectedPort(guid);

  await trustedClick();
  return promise_rejects_js(t, TypeError, navigator.serial.requestPort({
    filters: [{usbProductId: 0x0001}],
  }));
}, 'requestPort() requires a USB vendor ID if a product ID specified');
