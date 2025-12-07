(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Bluetooth adding and removing descriptor from a characteristic');
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
  await helper.setupGattOperationHandler();
  const {result: {serviceId: heartRateServiceId}} =
      await bp.BluetoothEmulation.addService({
        address: helper.peripheralAddress(),
        serviceUuid: BluetoothHelper.HEART_RATE_SERVICE_UUID,
      });
  const {result: {characteristicId: measurementIntervalCharacteristicId}} =
      await bp.BluetoothEmulation.addCharacteristic({
        serviceId: heartRateServiceId,
        characteristicUuid:
            BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        properties: {read: true}
      });

  const getDescriptors = async (serviceUuid, characteristicUuid) => {
    const devices = await navigator.bluetooth.getDevices();
    const server = await devices[0].gatt.connect();

    let descriptors = [];
    try {
      const service = await server.getPrimaryService(serviceUuid);
      const characteristic =
          await service.getCharacteristic(characteristicUuid);
      descriptors = await characteristic.getDescriptors();
    } catch (e) {
      return e.message;
    }
    return descriptors.map(c => [c.uuid]);
  };

  // Start the test.
  const {result: {descriptorId: userDescriptionDescriptorId}} =
      await bp.BluetoothEmulation.addDescriptor({
        characteristicId: measurementIntervalCharacteristicId,
        descriptorUuid:
            BluetoothHelper.CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
      });
  testRunner.log(`After adding characteristic user description descriptor: ${
      await session.evaluateAsync(
          getDescriptors, BluetoothHelper.HEART_RATE_SERVICE_UUID,
          BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)}`);

  const {result: {descriptorId: clientConfigurationDescriptorId}} =
      await bp.BluetoothEmulation.addDescriptor({
        characteristicId: measurementIntervalCharacteristicId,
        descriptorUuid:
            BluetoothHelper.CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID
      });
  testRunner.log(
      `After adding client characteristic configuration descriptor: ${
          await session.evaluateAsync(
              getDescriptors, BluetoothHelper.HEART_RATE_SERVICE_UUID,
              BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)}`);

  await bp.BluetoothEmulation.removeDescriptor({
    descriptorId: clientConfigurationDescriptorId
  });
  testRunner.log(
      `After removing client characteristic configuration descriptor: ${
          await session.evaluateAsync(
              getDescriptors, BluetoothHelper.HEART_RATE_SERVICE_UUID,
              BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)}`);

  await bp.BluetoothEmulation.removeDescriptor({
    descriptorId: userDescriptionDescriptorId
  });
  testRunner.log(`After removing characteristic user description descriptor: ${
      await session.evaluateAsync(
          getDescriptors, BluetoothHelper.HEART_RATE_SERVICE_UUID,
          BluetoothHelper.MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)}`);

  testRunner.completeTest();
});
