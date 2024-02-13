// META: script=/resources/test-only-api.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);
  assert_false(device.opened);

  await device.close();
  assert_false(device.opened);
}, 'Closing a closed device is not an error');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);
  assert_false(device.opened);

  await device.open();
  assert_true(device.opened);

  device.close();
  assert_false(device.opened);

  await device.open();
  assert_true(device.opened);
}, 'Opening, closing, then opening again works');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);
  assert_false(device.opened);

  fake.simulateConnectFailure();
  await promise_rejects_dom(t, 'NotAllowedError', device.open());
  assert_false(device.opened);

  await device.open();
  assert_true(device.opened);
}, 'Opening after an error works');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);
  assert_false(device.opened);

  await device.open();
  return promise_rejects_dom(t, 'InvalidStateError', device.open());
}, 'Opening a device twice is an error');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);
  assert_false(device.opened);

  const firstRequest = device.open();
  await promise_rejects_dom(t, 'InvalidStateError', device.open());
  await firstRequest;
}, 'Opening a device twice simultaneously is an error');
