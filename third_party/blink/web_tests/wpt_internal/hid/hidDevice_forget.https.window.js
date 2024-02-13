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

  let devicesFirst = await navigator.hid.getDevices();
  assert_equals(devicesFirst.length, 1);
  assert_true(devicesFirst[0] instanceof HIDDevice);

  await device.forget();

  let devicesSecond = await navigator.hid.getDevices();
  assert_equals(devicesSecond.length, 0);
}, 'A forgotten device is not available in getDevices() anymore');

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
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 2);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);

  let devicesFirst = await navigator.hid.getDevices();
  assert_equals(devicesFirst.length, 2);
  assert_true(devicesFirst[0] instanceof HIDDevice);
  assert_true(devicesFirst[1] instanceof HIDDevice);

  await device.forget();

  let devicesSecond = await navigator.hid.getDevices();
  assert_equals(devicesSecond.length, 0);
}, 'Forgetting a device with multiple interfaces revokes access to all interfaces');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  const devices = await navigator.hid.requestDevice({filters: []});
  assert_true(devices instanceof Array);
  assert_equals(devices.length, 1);

  const device = devices[0];
  assert_true(device instanceof HIDDevice);

  await device.forget();
  await device.forget();
}, 'Forgetting a device twice is not an error');

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
  await promise_rejects_dom(t, 'InvalidStateError', device.forget());
  await firstRequest;
}, 'Forgetting a device while it is opening is an error');

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

  const firstRequest = device.forget();
  await promise_rejects_dom(t, 'InvalidStateError', device.close());
  await firstRequest;
}, 'Closing a device while it is forgetting is an error');

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

  await device.forget();
  await promise_rejects_dom(t, 'InvalidStateError', device.close());
}, 'Closing a device when it is forgotten is an error');

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

  const firstRequest = device.forget();
  await promise_rejects_dom(t, 'InvalidStateError', device.open());
  await firstRequest;
}, 'Opening a device while it is forgetting is an error');

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

  await device.forget();
  await promise_rejects_dom(t, 'InvalidStateError', device.open());
}, 'Opening a device when it is forgotten is an error');

hid_test(async (t, fake) => {
  const device = fake.makeDevice(kTestVendorId, kTestProductId);

  // Disconnect device after granting access to it.
  const guid = fake.addDevice(device, /*grantPermission=*/ true);
  fake.removeDevice(guid);
  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 1);

  // Forget device after disconnecting it.
  await devices[0].forget();
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);

  // Connect device again.
  fake.addDevice(device, /*grantPermission=*/ false);
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);
}, 'Permission is not remembered even after reconnection');

hid_test(async (t, fake) => {
  const guid = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.setSelectedDevice(guid);

  await trustedClick();
  let devices = await navigator.hid.requestDevice({filters: []});
  assert_equals(devices.length, 1);

  await devices[0].forget();

  await trustedClick();
  devices = await navigator.hid.requestDevice({filters: []});
  assert_equals(devices.length, 1);

  await devices[0].open();
  assert_true(devices[0].opened);
}, 'Opening a device that has been forgotten previously is possible');
