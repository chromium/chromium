// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/generic-sensor/resources/generic-sensor-helpers.js

'use strict';

// This test relies on Chromium specific behavior: when a page cannot expose
// readings (like here, when it does not have focus), the platform sensor will
// be suspended if there are no other users.
promise_test(async t => {
  const sensorName = 'gyroscope';
  const reading = {x: 1, y: 2, z: 3};
  assert_implements(window.internals, 'window.internals is required');
  await test_driver.set_permission({name: sensorName}, 'granted');
  await test_driver.create_virtual_sensor(sensorName);

  const sensor = new Gyroscope();
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor(sensorName);
  });
  const sensorWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);
  sensor.start();
  await sensorWatcher.wait_for('activate');

  const sensorInfo =
      await test_driver.get_virtual_sensor_information(sensorName);
  const sensorPeriodInMs = (1 / sensorInfo.requestedSamplingFrequency) * 1000;

  window.internals.setFocused(false);
  await test_driver.update_virtual_sensor(sensorName, reading);
  // Wait to make sure that no "reading" even has been delivered.
  await new Promise(resolve => {t.step_timeout(resolve, sensorPeriodInMs * 2)});
  assert_true(sensor.activated);
  assert_false(sensor.hasReading);
  assert_sensor_reading_is_null(sensor);

  window.internals.setFocused(true);
  await Promise.all([
    test_driver.update_virtual_sensor(sensorName, reading),
    sensorWatcher.wait_for('reading'),
  ]);
  assert_true(sensor.activated);
  assert_true(sensor.hasReading);
  assert_not_equals(sensor.timestamp, null);
}, 'Losing focus must cause readings to be suspended');
