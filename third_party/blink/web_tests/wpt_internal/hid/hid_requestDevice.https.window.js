// META: script=/resources/test-only-api.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;
const kTestUsage = 0x0001;

promise_test((t) => {
  return promise_rejects_dom(
      t, 'SecurityError', navigator.hid.requestDevice({filters: []}));
}, 'requestDevice() rejects without a user gesture');

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
      t, TypeError, navigator.hid.requestDevice({filters: [{}]}));
}, 'requestDevice() rejects with an empty filter');

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({
      filters: [{ productId: kTestProductId }],
    })
  );
}, "requestDevice() rejects with a productId value only in a filter");

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({
      filters: [{ usage: kTestUsage }],
    })
  );
}, "requestDevice() rejects with an usage value only in a filter");

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({ filters: [], exclusionFilters: [] })
  );
}, "requestDevice() rejects with empty exclusion filters");

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({ filters: [], exclusionFilters: [{}] })
  );
}, "requestDevice() rejects with an empty exclusion filter");

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({
      filters: [],
      exclusionFilters: [{ productId: kTestProductId }],
    })
  );
}, "requestDevice() rejects with a productId value only in an exclusion filter");

promise_test(async (t) => {
  await trustedClick();
  return promise_rejects_js(
    t,
    TypeError,
    navigator.hid.requestDevice({
      filters: [],
      exclusionFilters: [{ usage: kTestUsage }],
    })
  );
}, "requestDevice() rejects with an usage value only in an exclusion filter");

promise_test(async (t) => {
  const {HidService} =
      await import('/gen/third_party/blink/public/mojom/hid/hid.mojom.m.js');
  let interceptor = new MojoInterfaceInterceptor(HidService.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  await trustedClick();
  let devices = null;
  try {
    devices = await navigator.hid.requestDevice({filters: []});
  } finally {
    interceptor.stop();
  }
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 0);
}, 'requestDevice() returns an empty array if Mojo service connection fails');

hid_test(async (t, fake) => {
  await trustedClick();
  let devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 0);
}, 'requestDevice() returns an empty array if no device has been selected');

hid_test(async (t, fake) => {
  let guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  let devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);
  assert_true(devices[0] instanceof HIDDevice);
}, 'requestDevice() returns the selected device');

hid_test(async (t, fake) => {
  let guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  let firstDevices = await navigator.hid.requestDevice({filters: []});
  assert_true(firstDevices instanceof Array);
  assert_equals(firstDevices.length, 1);
  assert_true(firstDevices[0] instanceof HIDDevice);
  let secondDevices = await navigator.hid.requestDevice({filters: []});
  assert_false(firstDevices === secondDevices);
  assert_true(secondDevices instanceof Array);
  assert_equals(secondDevices.length, 1);
  assert_true(secondDevices[0] instanceof HIDDevice);
  assert_true(firstDevices[0] === secondDevices[0]);
}, 'requestDevice() returns the same device object every time');

hid_test(async (t, fake) => {
  // Construct two HidDeviceInfo objects with the same physical device ID to
  // simulate a device with two HID interfaces.
  let device0 = fake.makeDevice(kTestVendorId, kTestProductId);
  let device1 = fake.makeDevice(kTestVendorId, kTestProductId);
  device1.physicalDeviceId = device0.physicalDeviceId;
  let key0 = fake.addDevice(device0);
  let key1 = fake.addDevice(device1);
  assert_equals(key0, key1);
  fake.setSelectedDevice(key0);

  await trustedClick();
  let devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(2, devices.length);
  assert_true(devices[0] instanceof HIDDevice);
  assert_true(devices[1] instanceof HIDDevice);
  assert_false(devices[0] === devices[1]);
}, 'requestDevice() returns 2 HIDDevices for a device with 2 HID interfaces');

hid_test(async (t, fake) => {
  // Construct two HidDeviceInfo objects with empty physical device IDs.
  let device0 = fake.makeDevice(kTestVendorId, kTestProductId);
  let device1 = fake.makeDevice(kTestVendorId, kTestProductId);
  device0.physicalDeviceId = '';
  device1.physicalDeviceId = '';
  let key0 = fake.addDevice(device0);
  let key1 = fake.addDevice(device1);
  assert_not_equals('', key0);
  assert_not_equals(key1, key0);
  fake.setSelectedDevice(key0);

  await trustedClick();
  let devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(1, devices.length);
  assert_true(devices[0] instanceof HIDDevice);
}, 'requestDevice() does not merge devices with empty physical device IDs');
