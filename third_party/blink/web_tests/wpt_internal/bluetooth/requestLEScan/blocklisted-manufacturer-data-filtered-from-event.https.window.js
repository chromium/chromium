// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/bluetooth/resources/bluetooth-test.js
// META: script=/bluetooth/resources/bluetooth-fake-devices.js
// META: timeout=long
'use strict';
const test_desc = `Blocked manufacturer data, service UUIDs and service data ` +
    `are filtered from the advertisement event in requestLEScan.`;

const blocklistedServiceUUID = BluetoothUUID.getService(0x1812);
const blocklistedServiceData = new Uint8Array([1, 2, 3]);

const advertisement_packet_with_blocked_data = {
  deviceAddress: '07:07:07:07:07:07',
  rssi: -10,
  scanRecord: {
    name: 'LE Device',
    uuids: [uuid1234, blocklistedServiceUUID],
    manufacturerData: {
      [nonBlocklistedManufacturerId]: nonBlocklistedManufacturerData,
      [blocklistedManufacturerId]: blocklistedManufacturerData,
    },
    serviceData: {
      [uuid1234]: uuid1234Data,
      [blocklistedServiceUUID]: blocklistedServiceData,
    },
  }
};

bluetooth_test(async (t) => {
  const fake_central =
      await navigator.bluetooth.test.simulateCentral({state: 'powered-on'});

  const watcher =
      new EventWatcher(t, navigator.bluetooth, ['advertisementreceived']);

  await callWithTrustedClick(async () => {
    await navigator.bluetooth.requestLEScan({
      filters: [{services: [uuid1234]}],
    });
  });

  let advertisementreceivedPromise = watcher.wait_for('advertisementreceived');
  fake_central.simulateAdvertisementReceived(
      advertisement_packet_with_blocked_data);
  let evt = await advertisementreceivedPromise;

  // Check if non blocked-listed manufacturer still exists.
  assert_true(
      evt.manufacturerData.has(nonBlocklistedManufacturerId),
      'Non-blocklisted manufacturer data should be present.');
  assert_data_maps_equal(
      evt.manufacturerData, /*expected_key=*/ nonBlocklistedManufacturerId,
      nonBlocklistedManufacturerData);

  // Check if block-listed manufacturer data is filtered out properly.
  assert_false(
      evt.manufacturerData.has(blocklistedManufacturerId),
      'Blocklisted manufacturer data should be filtered out.');

  // Check if non blocked-listed service UUID still exists.
  assert_true(
      evt.uuids.includes(uuid1234),
      'Non-blocklisted service UUID should be present.');

  // Check if block-listed service UUID is filtered out properly.
  assert_false(
      evt.uuids.includes(blocklistedServiceUUID),
      'Blocklisted service UUID should be filtered out.');

  // Check if non blocked-listed service data still exists.
  assert_true(
      evt.serviceData.has(uuid1234),
      'Non-blocklisted service data should be present.');
  assert_data_maps_equal(
      evt.serviceData, /*expected_key=*/ uuid1234, uuid1234Data);

  // Check if block-listed service data is filtered out properly.
  assert_false(
      evt.serviceData.has(blocklistedServiceUUID),
      'Blocklisted service data should be filtered out.');
}, test_desc);
