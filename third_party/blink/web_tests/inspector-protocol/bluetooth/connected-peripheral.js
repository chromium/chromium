(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Bluetooth.simulateCentral is executed in the given world');
  const bp = testRunner.browserP();

  await dp.Page.enable();
  await dp.Runtime.enable();
  await bp.BluetoothEmulation.enable({state: 'powered-on'});
  const bluetoothAvailable = await session.evaluateAsync(
    () => navigator.bluetooth.getAvailability()
  );
  testRunner.log(bluetoothAvailable, undefined, 'Bluetooth availability');

  const fake_device_address = '09:09:09:09:09:09';
  const fake_device = await bp.BluetoothEmulation.simulatePreconnectedPeripheral({
    address: fake_device_address,
    name: 'Some Device',
    manufacturerData: [
      {
        key: 0x001,
        data: btoa(new Uint8Array([65, 66, 67]).buffer),
      }
    ],
    knownServiceUuids: [
      '12345678-1234-5678-9abc-def123456789',
    ],
  });

  await session.evaluateAsyncWithUserGesture(
    'navigator.bluetooth.requestLEScan({acceptAllAdvertisements: true})');

  const advertisementPromise = session.evaluateAsync(
    `new Promise(resolve => {
      navigator.bluetooth.addEventListener('advertisementreceived', ev => {
        resolve(ev.uuids);
      });
    });`
  );

  const advertisement_packet = {
    entry: {
      deviceAddress: '08:08:08:08:08:08',
      rssi: -10,
      scanRecord: {
        name: 'Heart Rate',
        uuids: ['0000180d-0000-1000-8000-00805f9b34fb'],
        manufacturerData: fake_device.manufacturer_data,
        appearance: 1,
        txPower: 1
      },
    }
  };

  bp.BluetoothEmulation.simulateAdvertisement(advertisement_packet);
  const results = await advertisementPromise;

  testRunner.log(results);
  testRunner.completeTest();
});