(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp, page} = await testRunner.startBlank(
      'Tests the data of invalidation tracking trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  let tracingHelper = new TracingHelper(testRunner, session);
  let scheduleStyleInvalidationTracking;
  let styleRecalcInvalidationTracking;
  let styleInvalidatorInvalidationTracking;
  let layoutInvalidationTracking;
  let layout;

  const MAX_ATTEMPTS = 3;
  for (let attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
    await tracingHelper.startTracing(
        'disabled-by-default-devtools.timeline.invalidationTracking,devtools.timeline,disabled-by-default-devtools.timeline.stack');
    await dp.Page.navigate({
      url: 'http://127.0.0.1:8000/inspector-protocol/resources/web-vitals.html'
    });

    // Wait for trace events.
    await session.evaluateAsync(`
      new Promise((res) => {
        (new PerformanceObserver(res)).observe({entryTypes: ['layout-shift']});
      })`);

    await tracingHelper.stopTracing(
        /(disabled-by-default-)?devtools\.timeline(\.invalidationTracking|stack)?/);

    scheduleStyleInvalidationTracking = tracingHelper.findEvents(
        'ScheduleStyleInvalidationTracking', Phase.INSTANT)[0];
    styleRecalcInvalidationTracking = tracingHelper.findEvents(
        'StyleRecalcInvalidationTracking', Phase.INSTANT);
    styleInvalidatorInvalidationTracking = tracingHelper.findEvents(
        'StyleInvalidatorInvalidationTracking', Phase.INSTANT)[0];
    layoutInvalidationTracking = tracingHelper.findEvents(
        'LayoutInvalidationTracking', Phase.INSTANT)[0];
    layout = tracingHelper.findEvents(
          'Layout', Phase.COMPLETE, e => e.args?.beginData?.stackTrace)[0];

    if (scheduleStyleInvalidationTracking && styleRecalcInvalidationTracking &&
        styleInvalidatorInvalidationTracking && layoutInvalidationTracking) {
      break;
    }
  }

  testRunner.log('ScheduleStyleInvalidationTracking');
  tracingHelper.logEventShape(scheduleStyleInvalidationTracking);

  testRunner.log('StyleRecalcInvalidationTracking');
  tracingHelper.logEventShape(styleRecalcInvalidationTracking);

  testRunner.log('StyleInvalidatorInvalidationTracking');
  tracingHelper.logEventShape(styleInvalidatorInvalidationTracking);

  testRunner.log('LayoutInvalidationTracking');
  tracingHelper.logEventShape(layoutInvalidationTracking);

  testRunner.log('Style recalc initiator:');
  testRunner.log(styleRecalcInvalidationTracking[0].args?.data?.stackTrace[0].functionName);

  testRunner.log('Layout initiator:');
  testRunner.log(layout.args?.beginData?.stackTrace[0].functionName);

  testRunner.completeTest();
})
