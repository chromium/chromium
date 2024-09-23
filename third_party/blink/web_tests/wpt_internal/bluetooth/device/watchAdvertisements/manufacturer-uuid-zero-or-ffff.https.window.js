// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/bluetooth/resources/bluetooth-test.js
// META: script=/bluetooth/resources/bluetooth-fake-devices.js
'use strict';
const test_desc =
    `Ensure Manufacturer UUIDs 0x0000 and 0xffff are valid and do not crash` +
    `renderer, refer to crbug.com/356891475 for more information`;

bluetooth_test(async (t) => {
  let {device} = await setUpPreconnectedFakeDevice({
    fakeDeviceOptions: {
      address: '07:07:07:07:07:07',
      knownServiceUUIDs: [uuid1234],
    },
    requestDeviceOptions: {
      filters: [{services: [uuid1234]}],
      optionalManufacturerData: [0x0000, 0xffff]
    }
  });
  const watcher = new EventWatcher(t, device, ['advertisementreceived']);

  await device.watchAdvertisements();
  assert_true(device.watchingAdvertisements);

  let advertisementreceivedPromise = watcher.wait_for('advertisementreceived');
  await fake_central.simulateAdvertisementReceived(
      zero_and_ffff_manufacturer_uuid_ad_packet);
  let evt = await advertisementreceivedPromise;
  assert_equals(evt.device, device);

  // Check that manufacturer id 0x0000 and 0xffff don't crash renderer.
  assert_data_maps_equal(
      evt.manufacturerData, /*expected_key=*/ 0x0000, manufacturer1Data);
  assert_data_maps_equal(
      evt.manufacturerData, /*expected_key=*/ 0xffff, manufacturer2Data);
}, test_desc);
