// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/generic-sensor/resources/generic-sensor-helpers.js
// META: script=/accelerometer/resources/sensor-data.js
// META: script=/gyroscope/resources/sensor-data.js
// META: script=/magnetometer/resources/sensor-data.js
// META: script=/orientation-sensor/resources/sensor-data.js

'use strict';

function runReferenceFrameTests(sensorData, readingData) {
  validate_sensor_data(sensorData);
  validate_reading_data(readingData);

  const {sensorName, permissionName, testDriverName, featurePolicyNames} =
      sensorData;
  const sensorType = self[sensorName];

  function sensor_test(func, name, properties) {
    promise_test(async t => {
      assert_implements(sensorName in self, `${sensorName} is not supported.`);

      const readings = new RingBuffer(readingData.readings);
      const expectedReadings = new RingBuffer(readingData.expectedReadings);
      const expectedRemappedReadings = readingData.expectedRemappedReadings ?
          new RingBuffer(readingData.expectedRemappedReadings) :
          undefined;

      return func(t, readings, expectedReadings, expectedRemappedReadings);
    }, name, properties);
  }

  sensor_test(async (t, readings, expectedReadings, expectedRemappedReadings) => {
    for (const orientation of ['portrait-secondary', 'landscape-secondary']) {
      if (screen.orientation.angle == 270)
        break;
      testRunner.setMockScreenOrientation(orientation);
    }

    assert_equals(
        screen.orientation.angle, 270,
        'Screen orientation angle must be set to 270.');

    await test_driver.set_permission({name: permissionName}, 'granted');

    await test_driver.create_virtual_sensor(testDriverName);

    const sensor1 = new sensorType({frequency: 60});
    const sensor2 = new sensorType({frequency: 60, referenceFrame: 'screen'});
    t.add_cleanup(async () => {
      sensor1.stop();
      sensor2.stop();
      await test_driver.remove_virtual_sensor(testDriverName);
    });
    const sensorWatcher1 =
        new EventWatcher(t, sensor1, ['activate', 'reading', 'error']);
    const sensorWatcher2 =
        new EventWatcher(t, sensor1, ['activate', 'reading', 'error']);

    sensor1.start();
    sensor2.start();

    await Promise.all([
      sensorWatcher1.wait_for('activate'), sensorWatcher2.wait_for('activate')
    ]);

    await Promise.all([
      test_driver.update_virtual_sensor(testDriverName, readings.next().value),
      sensorWatcher1.wait_for('reading'), sensorWatcher2.wait_for('reading')
    ]);

    const expected = expectedReadings.next().value;
    const expectedRemapped = expectedRemappedReadings.next().value;
    assert_sensor_reading_equals(sensor1, expected);
    assert_sensor_reading_equals(sensor2, expectedRemapped);

    sensor1.stop();
    assert_sensor_reading_is_null(sensor1);
    assert_sensor_reading_equals(sensor2, expectedRemapped);

    sensor2.stop();
    assert_sensor_reading_is_null(sensor2);
  }, `${sensorName}: sensor reading is correct when options.referenceFrame is 'screen'.`);
}

runReferenceFrameTests(kAccelerometerSensorData, kAccelerometerReadings);
runReferenceFrameTests(kGravitySensorData, kAccelerometerReadings);
runReferenceFrameTests(kLinearAccelerationSensorData, kAccelerometerReadings);

runReferenceFrameTests(kGyroscopeSensorData, kGyroscopeReadings);

runReferenceFrameTests(kMagnetometerSensorData, kMagnetometerReadings);

runReferenceFrameTests(kAbsoluteOrientationSensorData, kOrientationReadings);
runReferenceFrameTests(kRelativeOrientationSensorData, kOrientationReadings);
