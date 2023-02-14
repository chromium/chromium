// META: script=/generic-sensor/resources/generic-sensor-helpers.js

'use strict';

sensor_test(async (t, sensorProvider) => {
  assert_implements(window.internals, 'window.internals is required');

  const sensor = new Gyroscope();
  t.add_cleanup(() => {
    sensor.stop();
  });
  const sensorWatcher = new EventWatcher(t, sensor, ['reading', 'error']);
  sensor.start();

  const platformSensor = await sensorProvider.getCreatedSensor('Gyroscope');

  await sensorWatcher.wait_for('reading');
  assert_true(platformSensor.isReadingData());

  window.internals.setFocused(false);
  await t.step_wait(
      () => !platformSensor.isReadingData(), 'readings must be suspended');
}, 'Losing focus must cause readings to be suspended');
