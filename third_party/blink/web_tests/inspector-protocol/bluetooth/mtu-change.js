(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests simulating ATT MTU changes.');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js');
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  await helper.setupGattOperationHandler();
  await helper.requestDevice({acceptAllDevices: true});

  const connectedSize = await session.evaluateAsync(async () => {
    const [device] = await navigator.bluetooth.getDevices();
    window.server = device.gatt;
    await window.server.connect();
    return window.server.maxWriteWithoutResponseSize;
  });
  testRunner.log(`maxWriteWithoutResponseSize on connect: ${connectedSize}`);

  // The listener has to be in place before the MTU change is simulated.
  await session.evaluate(() => {
    window.sizeChanged = new Promise(
        resolve => window.server.addEventListener(
            'maxwritewithoutresponsesizechanged',
            () => resolve(window.server.maxWriteWithoutResponseSize),
            {once: true}));
  });
  await bp.BluetoothEmulation.simulateGATTMtuChange(
      {address: helper.peripheralAddress(), mtu: 100});
  const changedSize = await session.evaluateAsync(() => window.sizeChanged);
  testRunner.log(
      `maxWriteWithoutResponseSize after MTU change: ${changedSize}`);

  const {error: invalidMtuError} =
      await bp.BluetoothEmulation.simulateGATTMtuChange(
          {address: helper.peripheralAddress(), mtu: 10});
  testRunner.log(`Invalid MTU: ${invalidMtuError.message}`);

  const {error: unknownAddressError} =
      await bp.BluetoothEmulation.simulateGATTMtuChange(
          {address: '11:11:11:11:11:11', mtu: 100});
  testRunner.log(`Unknown peripheral: ${unknownAddressError.message}`);

  testRunner.completeTest();
});
