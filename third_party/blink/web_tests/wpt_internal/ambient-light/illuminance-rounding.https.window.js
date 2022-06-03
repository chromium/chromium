// META: script=/generic-sensor/resources/generic-sensor-helpers.js

'use strict';

sensor_test(async (t, sensorProvider) => {
  const sensor = new AmbientLightSensor({
    frequency: 10  // 10Hz is the maximum frequency allowed by the Blink
                   // implementation.
  });
  t.add_cleanup(() => {
    sensor.stop();
  });
  const eventWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);

  sensor.start();
  await eventWatcher.wait_for('activate');
  assert_false(
      sensor.hasReading, 'Sensor has no readings immediately after activation');
  assert_equals(
      sensor.illuminance, null,
      'Sensor must have no illuminance immediately after activation');

  const mockSensor =
      await sensorProvider.getCreatedSensor('AmbientLightSensor');
  await mockSensor.setSensorReading([[24], [35], [49], [35], [24]]);

  // This loop checks that illuminance rounding causes the following to happen:
  // 1. The first ever reading goes from nothing to 24. A new "reading" event is
  // emitted and the rounded illuminance value is 0.
  // 2. Going from 24 to 35 is not significant enough. No "reading" event is
  // emitted and the illuminance value remains the same.
  // 3. Going from 24 to 49 is significant enough. A "reading" event is emitted
  // and the rounded illuminance value is 50.
  // 4. Going from 49 to 35 is not significant enough. No "reading" event is
  // emitted and the illuminance value remains the same.
  // 5. Going from 49 to 24 is significant enough. A "reading" event is emitted
  // and the rounded illuminance value is 0.
  // 6. We are back to the first raw reading value. We are at 24 and get 24, so
  // nothing happens.
  // 7. Go to step 3.
  for (let i = 0; i < 3; i++) {
    await eventWatcher.wait_for('reading');
    assert_true(sensor.hasReading);
    assert_equals(sensor.illuminance, 0, 'Rounded illuminance should be 0');

    await eventWatcher.wait_for('reading');
    assert_true(sensor.hasReading);
    assert_equals(sensor.illuminance, 50, 'Rounded illuminance should be 50');
  }
}, 'Illuminance rounding');
