// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/generic-sensor/resources/generic-sensor-helpers.js

// TODO(crbug.com/1520919): Upstream this file to
// external/wpt/infrastructure/testdriver/virtual_sensors.https.html
// once https://github.com/web-platform-tests/wpt/pull/42885 is merged.

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');
  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});

  const sensor = new Accelerometer;
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  const sensorWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);

  sensor.start();
  const preActivationTimestamp = performance.now();
  await sensorWatcher.wait_for('activate');
  await sensorWatcher.wait_for('reading');

  assert_sensor_reading_equals(sensor, {x: 1, y: 2, z: 3});
  assert_less_than(sensor.timestamp, preActivationTimestamp);
}, 'Updates provided before a sensor is created are stashed');

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');

  const sensor = new Accelerometer;
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  const sensorWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);

  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});

  sensor.start();
  await sensorWatcher.wait_for(['activate', 'reading']);

  const timestamp = sensor.timestamp;

  sensor.stop();

  assert_equals(sensor.timestamp, null);

  sensor.start();
  await sensorWatcher.wait_for(['activate', 'reading']);

  assert_sensor_reading_equals(sensor, {x: 1, y: 2, z: 3});
  assert_equals(sensor.timestamp, timestamp);
}, 'Updates are sent automatically on sensor reactivation');

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');

  const sensor = new Accelerometer;
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  const sensorWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);

  sensor.start();
  await sensorWatcher.wait_for('activate');

  // Now stop the sensor and wait for it to reach the backend.
  // Contrary to the test that calls update_virtual_sensor()
  // immediately after create_virtual_sensor(), in this case the
  // sensor does exist but is not active.
  sensor.stop();
  await t.step_wait(async () => {
    const result =
        await test_driver.get_virtual_sensor_information('accelerometer');
    return result.requestedSamplingFrequency === 0;
  });

  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});

  sensor.start();
  const preActivationTimestamp = performance.now();
  await sensorWatcher.wait_for('activate');
  await sensorWatcher.wait_for('reading');

  assert_sensor_reading_equals(sensor, {x: 1, y: 2, z: 3});
  assert_less_than(sensor.timestamp, preActivationTimestamp);
}, 'Updates provided while sensor is stopped are stashed');

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');
  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});
  await test_driver.update_virtual_sensor(
      'accelerometer', {x: 10, y: 20, z: 30});

  const sensor = new Accelerometer;
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  const sensorWatcher = new EventWatcher(t, sensor, ['reading', 'error']);

  sensor.start();
  await sensorWatcher.wait_for('reading');

  assert_sensor_reading_equals(sensor, {x: 10, y: 20, z: 30});
}, 'Stashed readings overwrite previous ones');

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');

  await test_driver.create_virtual_sensor('accelerometer');
  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});
  await test_driver.remove_virtual_sensor('accelerometer');

  await test_driver.create_virtual_sensor('accelerometer');

  const sensor = new Accelerometer;
  t.add_cleanup(async () => {
    sensor.stop();
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  const sensorWatcher =
      new EventWatcher(t, sensor, ['activate', 'reading', 'error']);

  sensor.start();
  await sensorWatcher.wait_for('activate');

  // Wait to make sure no "reading" event is delivered, as the update was sent
  // before the virtual sensor was removed.
  await new Promise(resolve => {t.step_timeout(resolve, 700)});
}, 'Stashed readings do not persist across virtual sensor creations');

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');
  t.add_cleanup(async () => {
    await test_driver.remove_virtual_sensor('accelerometer');
  });
  await test_driver.update_virtual_sensor('accelerometer', {x: 1, y: 2, z: 3});

  const iframe = document.createElement('iframe');
  iframe.src = 'https://{{host}}:{{ports[https][0]}}/resources/blank.html';
  const iframeLoadWatcher = new EventWatcher(t, iframe, 'load');
  t.add_cleanup(() => {
    if (iframe.parentNode) {
      iframe.parentNode.removeChild(iframe);
    }
  });

  async function createAndTestIframeSensor() {
    document.body.appendChild(iframe);
    await iframeLoadWatcher.wait_for('load');
    const iframeSensor = new iframe.contentWindow['Accelerometer']();
    t.add_cleanup(() => {
      iframeSensor.stop();
    });
    const iframeSensorWatcher =
        new EventWatcher(t, iframeSensor, ['activate', 'error', 'reading']);

    iframe.contentWindow.focus();
    t.add_cleanup(() => {
      // Make sure the main window is focused again so that other tests do not
      // fail.
      window.focus();
    });
    iframeSensor.start();
    await iframeSensorWatcher.wait_for(['activate', 'reading']);
    assert_sensor_reading_equals(iframeSensor, {x: 1, y: 2, z: 3});
    iframeSensor.stop();
  }

  // We want to test that virtual sensor readings persist until
  // remove_virtual_sensor() is called, even if the entire frame that was using
  // a Sensor is removed.
  await createAndTestIframeSensor();
  iframe.parentNode.removeChild(iframe);
  await createAndTestIframeSensor();
}, 'Updates persist when frame with Sensor is removed');
