(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of invalidation tracking trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline.invalidationTracking');
  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/web-vitals.html'
  });

  // Wait for trace events.
  await session.evaluateAsync(`
      new Promise((res) => {
        (new PerformanceObserver(res)).observe({entryTypes: ['layout-shift']});
      })`);

  await tracingHelper.stopTracing(
      /disabled-by-default-devtools\.timeline\.invalidationTracking/);

  const scheduleStyleInvalidationTracking = tracingHelper.findEvent(
      'ScheduleStyleInvalidationTracking', Phase.INSTANT);
  const styleRecalcInvalidationTracking = tracingHelper.findEvents(
      'StyleRecalcInvalidationTracking', Phase.INSTANT);
  const styleInvalidatorInvalidationTracking = tracingHelper.findEvent(
      'StyleInvalidatorInvalidationTracking', Phase.INSTANT);

  const layoutInvalidationTracking =
      tracingHelper.findEvent('LayoutInvalidationTracking', Phase.INSTANT);

  testRunner.log('ScheduleStyleInvalidationTracking');
  tracingHelper.logEventShape(scheduleStyleInvalidationTracking);

  testRunner.log('StyleRecalcInvalidationTracking');
  tracingHelper.logEventShape(styleRecalcInvalidationTracking);

  testRunner.log('StyleInvalidatorInvalidationTracking');
  tracingHelper.logEventShape(styleInvalidatorInvalidationTracking);

  testRunner.log('LayoutInvalidationTracking');
  tracingHelper.logEventShape(layoutInvalidationTracking);

  testRunner.completeTest();
})
