// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

// Test for https://crbug.com/324301018, which is Blink-specific: there is a
// short window between Sensor::HandleError() and Sensor::NotifyError() during
// which Sensor::ContextDestroyed() may be called. The latter would set
// |state_| to SensorState::kIdle, causing NotifyError() to trip on its CHECK.

promise_test(async t => {
  await test_driver.set_permission({name: 'accelerometer'}, 'granted');
  await test_driver.create_virtual_sensor('accelerometer');

  const iframe = document.createElement('iframe');
  iframe.src = 'https://{{host}}:{{ports[https][0]}}/resources/blank.html';
  t.add_cleanup(async () => {
    if (iframeSensor) {
      iframeSensor.stop();
    }
    if (iframe.parentNode) {
      iframe.parentNode.removeChild(iframe);
    }
    await test_driver.remove_virtual_sensor('accelerometer');
  });

  document.body.appendChild(iframe);
  await new EventWatcher(t, iframe, 'load').wait_for('load');
  const iframeSensor = new iframe.contentWindow['Accelerometer']();
  const iframeSensorWatcher =
      new EventWatcher(t, iframeSensor, ['activate', 'error']);

  iframe.contentWindow.focus();
  iframeSensor.start();
  await iframeSensorWatcher.wait_for(['activate']);

  // The following set of steps is problematic and leads to a crash:
  // 1. remove_virtual_sensor() reaches
  //    VirtualPlatformSensor::SimulateSensorRemoval().
  // 2. PlatformSensor::NotifySensorError() eventually reaches
  //    SensorProxyImpl::ReportError(), which in turn eventually reaches
  //    Sensor::HandleError().
  // 3. Sensor::pending_error_notification_ is set.
  // 4. The removeChild() call below reaches Sensor::ContextDestroyed().
  // 5. Sensor::ContextDestroyed() sets |state_| to SensorState::kIdle.
  // 6. Sensor::NotifyError() is reached and hits a CHECK.
  //
  // Triggering this specific sequence from a web test is tricky (in
  // particular making sure that step 5 happens before step 6), but the code
  // below can do it reliably on Linux as of r1256676.
  await test_driver.remove_virtual_sensor('accelerometer');
  await new Promise(resolve => {
    t.step_timeout(() => {
      iframe.parentNode.removeChild(iframe);
      resolve();
    }, 0);
  });
}, 'Sensor::ContextDestroyed() with an in-progress error notification does not crash');
