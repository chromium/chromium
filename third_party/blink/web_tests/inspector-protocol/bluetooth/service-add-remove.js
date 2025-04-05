(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth adding and removing service from a pheripheral');
  const bp = testRunner.browserP();
  await dp.Page.enable();
  await dp.Runtime.enable();
  const BluetoothHelper =
      await testRunner.loadScript('resources/bluetooth-helper.js')
  const helper = new BluetoothHelper(testRunner, dp, session);
  await helper.setupPreconnectedPeripheral();
  await helper.requestDevice({
    acceptAllDevices: true,
    optionalServices: [
      BluetoothHelper.HEART_RATE_SERVICE_UUID,
      BluetoothHelper.BATTERY_SERVICE_UUID
    ]
  });
  await helper.setupGattOperationHandler();

  const getPrimaryServices = async () => {
    const devices = await navigator.bluetooth.getDevices();
    const server = await devices[0].gatt.connect();
    try {
      const services = await server.getPrimaryServices();
      return services.map(s => s.uuid);
    } catch (e) {
      return e.message;
    }
  };

  // Start the test.
  const {result: {serviceId: heartRateServiceId}} =
      await bp.BluetoothEmulation.addService({
        address: helper.peripheralAddress(),
        serviceUuid: BluetoothHelper.HEART_RATE_SERVICE_UUID,
      });
  testRunner.log(`After adding heart rate service: ${
      await session.evaluateAsync(getPrimaryServices)}`);

  const {result: {serviceId: batteryServiceId}} =
      await bp.BluetoothEmulation.addService({
        address: helper.peripheralAddress(),
        serviceUuid: BluetoothHelper.BATTERY_SERVICE_UUID,
      });
  testRunner.log(`After adding battery service: ${
      await session.evaluateAsync(getPrimaryServices)}`);

  await bp.BluetoothEmulation.removeService({
    serviceId: batteryServiceId,
  });
  testRunner.log(`After removing battery service: ${
      await session.evaluateAsync(getPrimaryServices)}`);

  await bp.BluetoothEmulation.removeService({
    serviceId: heartRateServiceId,
  });
  testRunner.log(`After removing heart rate service: ${
      await session.evaluateAsync(getPrimaryServices)}`);

  testRunner.completeTest();
});
