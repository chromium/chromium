// META: timeout=long
// META: script=/compute-pressure/resources/pressure-helpers.js
// META: script=/resources/test-only-api.js

'use strict';

pressure_test(async (t, mockPressureService) => {
  const observerChanges = [];
  const observer = new PressureObserver(changes => {
    observerChanges.push(changes);
  });
  t.add_cleanup(() => {
    observer.disconnect();
  });
  observer.observe('cpu');
  mockPressureService.setPressureUpdate('cpu', 'critical');
  mockPressureService.startPlatformCollector(/*sampleRate*/ 5.0);
  await t.step_wait(
      () => observerChanges.length > 0, 'observer should receive data');
  assert_equals(observerChanges.length, 1);
  assert_equals(observerChanges[0][0].source, 'cpu');
  assert_equals(observerChanges[0][0].state, 'critical');

  window.internals.setFocused(false);
  mockPressureService.setPressureUpdate('cpu', 'nominal');
  await new Promise(resolve => t.step_timeout(resolve, 1000));
  assert_equals(observerChanges.length, 1);

  window.internals.setFocused(true);
  await t.step_wait(
      () => observerChanges.length > 1, 'observer should receive data');
  assert_equals(observerChanges.length, 2);
  assert_equals(observerChanges[1][0].source, 'cpu');
  assert_equals(observerChanges[1][0].state, 'nominal');
}, 'Observer should not receive PressureRecord if page loses focus');
