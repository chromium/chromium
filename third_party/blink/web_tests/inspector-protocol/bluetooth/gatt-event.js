(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth GATT operation events handling');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();

  // Set up Bluetooth emulation with a preconnected peripheral.
  await bp.BluetoothEmulation.enable({state: 'powered-on', leSupported: true});
  await bp.BluetoothEmulation.simulatePreconnectedPeripheral({
    address: '09:09:09:09:09:09',
    name: 'Some Device',
    manufacturerData: [],
    knownServiceUuids: [],
  });

  // Prepare device request prompt handling.
  await dp.DeviceAccess.enable();
  const deviceRequestPromptedPromise =
      dp.DeviceAccess.onceDeviceRequestPrompted().then(
          ({params: {id, devices}}) => {
            const deviceId = devices[0].id;
            return dp.DeviceAccess.selectPrompt({id, deviceId});
          })

  // Prepare GATT operation handling.
  bp.BluetoothEmulation.onGattOperationReceived(({params: {address, type}}) => {
    testRunner.log(
        `GATT operation received: address: ${address}, type: ${type}`);
    return bp.BluetoothEmulation.simulateGATTOperationResponse(
        {address: address, type: type, code: 0});
  });

  // Start the test.
  const gattConnectedPromise =
      session.evaluateAsyncWithUserGesture(async () => {
        const device = await navigator.bluetooth.requestDevice(
            {acceptAllDevices: true, optionalServices: ['heart_rate']});
        const server = await device.gatt.connect();
        return server.connected;
      });
  await deviceRequestPromptedPromise;
  const gattConnected = await gattConnectedPromise;
  testRunner.log(`GATT connect result: ${gattConnected}`);

  const getPrimaryServicesResult = await session.evaluateAsync(async () => {
    const devices = await navigator.bluetooth.getDevices();
    try {
      await devices[0].gatt.getPrimaryServices();
    } catch (e) {
      return e.message;
    }
  });
  testRunner.log(`Get primary services result: ${getPrimaryServicesResult}`);
  testRunner.completeTest();
});
