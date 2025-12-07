(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth GATT operation events handling');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();

  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  await helper.requestDevice({
    acceptAllDevices: true,
    optionalServices: [BluetoothHelper.HEART_RATE_SERVICE_UUID]
  });

  // Prepare GATT operation handling.
  bp.BluetoothEmulation.onGattOperationReceived(({params: {address, type}}) => {
    testRunner.log(
        `GATT operation received: address: ${address}, type: ${type}`);
    return bp.BluetoothEmulation.simulateGATTOperationResponse(
        {address: address, type: type, code: BluetoothHelper.HCI_SUCCESS});
  });

  // Start the test.
  const gattDisconnectedPromise = session.evaluateAsync(async () => {
    const devices = await navigator.bluetooth.getDevices();
    return new Promise((resolve) => {
      devices[0].addEventListener('gattserverdisconnected', resolve);
    });
  });
  const gattConnectedPromise =
      session.evaluateAsyncWithUserGesture(async () => {
        const devices = await navigator.bluetooth.getDevices();
        const server = await devices[0].gatt.connect();
        return server.connected;
      });
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

  await bp.BluetoothEmulation.simulateGATTDisconnection(
      {address: BluetoothHelper.PRECONNECTED_PERIPHERAL_ADDRESS});
  await gattDisconnectedPromise;
  const gattConnectedResult = await session.evaluateAsync(async () => {
    const devices = await navigator.bluetooth.getDevices();
    return devices[0].gatt.connected;
  });
  testRunner.log(`Get GATT connected result: ${gattConnectedResult}`);
  testRunner.completeTest();
});
