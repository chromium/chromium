// META: script=/resources/test-only-api.js
// META: script=/webhid/resources/common.js
// META: script=resources/automation.js
'use strict';

const kTestVendorId = 0x1234;
const kTestProductId = 0xabcd;

promise_test(async () => {
  const {HidService} =
      await import('/gen/third_party/blink/public/mojom/hid/hid.mojom.m.js');

  let interceptor = new MojoInterfaceInterceptor(HidService.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 0);

  interceptor.stop();
}, 'getDevices() returns empty list if Mojo service connection fails');

hid_test(async (t, fake) => {
  fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));
  fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));

  let devices = await navigator.hid.getDevices();
  assert_equals(devices.length, 2);
  assert_true(devices[0] instanceof HIDDevice);
  assert_true(devices[1] instanceof HIDDevice);
}, 'getDevices() returns the set of configured fake devices');

hid_test(async (t, fake) => {
  fake.addDevice(fake.makeDevice(kTestVendorId, kTestProductId));

  let devicesFirst = await navigator.hid.getDevices();
  assert_equals(devicesFirst.length, 1, 'first call returns one device');
  assert_true(devicesFirst[0] instanceof HIDDevice);
  let devicesSecond = await navigator.hid.getDevices();
  assert_equals(devicesSecond.length, 1, 'second call returns one device');
  assert_true(devicesSecond[0] instanceof HIDDevice);
  assert_true(devicesFirst[0] === devicesSecond[0]);
}, 'getDevices() returns the same device objects every time');
