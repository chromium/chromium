// META: script=/resources/test-only-api.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;

hid_test(async (t, fake) => {
  const watcher = new EventWatcher(t, navigator.hid, ['connect', 'disconnect']);

  // Wait for getDevices() to resolve in order to ensure that the Mojo client
  // interface has been configured.
  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);

  // Connect a device and wait for the connect event.
  let key = fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  let connectEvent = await watcher.wait_for('connect');

  assert_true(connectEvent instanceof HIDConnectionEvent);
  assert_true(connectEvent.device instanceof HIDDevice);
  assert_equals(kTestVendorId, connectEvent.device.vendorId);
  assert_equals(kTestProductId, connectEvent.device.productId);

  // Disconnect the device and wait for the disconnect event.
  fake.removeDevice(key);
  let disconnectEvent = await watcher.wait_for('disconnect');

  assert_true(disconnectEvent instanceof HIDConnectionEvent);
  assert_true(disconnectEvent.device instanceof HIDDevice);
  assert_equals(kTestVendorId, disconnectEvent.device.vendorId);
  assert_equals(kTestProductId, disconnectEvent.device.productId);
}, 'HID dispatches connection and disconnection events');

hid_test(async (t, fake) => {
  const {HidCollectionInfo, HidUsageAndPage} =
      await import('/gen/services/device/public/mojom/hid.mojom.m.js');
  const watcher = new EventWatcher(t, navigator.hid, ['connect', 'disconnect']);

  // Wait for getDevices() to resolve in order to ensure that the Mojo client
  // interface has been configured.
  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);

  // Connect a device with no collections.
  let device = fake.makeDevice(kTestVendorId, kTestProductId);
  let key = fake.addDevice(device);
  let connectEvent = await watcher.wait_for('connect');
  assert_equals(connectEvent.device.collections.length, 0);

  // Check that the device was added and has no collections.
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 1);
  assert_equals(devices[0].collections.length, 0);

  // Update the device with a new collection.
  let updatedDevice = fake.makeDevice(kTestVendorId, kTestProductId);
  updatedDevice.guid = device.guid;
  updatedDevice.physicalDeviceId = device.physicalDeviceId;
  let collection = new HidCollectionInfo();
  collection.usage = new HidUsageAndPage(1, 1);
  collection.reportIds = new Uint8Array();
  collection.inputReports = [];
  collection.outputReports = [];
  collection.featureReports = [];
  collection.children = [];
  updatedDevice.collections.push(collection);
  fake.changeDevice(updatedDevice);

  // Check that the device now has a collection.
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 1);
  assert_equals(devices[0].collections.length, 1);

  // Disconnect the device and wait for the disconnect event. The event should
  // have the updated device info.
  fake.removeDevice(key);
  let disconnectEvent = await watcher.wait_for('disconnect');
  assert_equals(disconnectEvent.device.collections.length, 1);
}, 'HID updates device info');

hid_test(async (t, fake) => {
  const watcher = new EventWatcher(t, navigator.hid, ['connect', 'disconnect']);

  // Wait for getDevices() to resolve in order to ensure that the Mojo client
  // interface has been configured.
  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);

  // Update device info for a device that was not previously added. A connect
  // event should be dispatched.
  let device = fake.makeDevice(kTestVendorId, kTestProductId);
  let key = fake.changeDevice(device);
  await watcher.wait_for('connect');

  // Check that the device was added.
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 1);

  // Disconnect the device and wait for the disconnect event.
  fake.removeDevice(key);
  await watcher.wait_for('disconnect');

  // Check that the device is not removed.
  devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 1);
}, 'HID dispatches connect for DeviceChanged without DeviceAdded');
