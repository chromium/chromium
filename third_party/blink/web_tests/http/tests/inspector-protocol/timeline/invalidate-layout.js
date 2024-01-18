(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that an InvalidateLayout trace event is dispatched at LocalFrameView::ScheduleRelayout');

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();

  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/relayout.html'});

  // Wait for trace events.
  await new Promise(resolve => setTimeout(resolve, 1000))

  const devtoolsEvents = await tracingHelper.stopTracing();
  const invalidateLayoutEvents = devtoolsEvents.filter(event => event.name === 'InvalidateLayout');
  if (!invalidateLayoutEvents.length) {
    testRunner.log('FAIL: did not receive any InvalidateRelayout event');
    testRunner.completeTest();
    return;
  }

  const invalidateLayoutEvent = invalidateLayoutEvents[0];
  testRunner.log('Trace event received');
  testRunner.log(`Event name: ${invalidateLayoutEvent.name}`);
  testRunner.log(`Type of event frame: ${typeof invalidateLayoutEvent.args.data.frame}`);
  testRunner.log(`Type of event nodeId: ${typeof invalidateLayoutEvent.args.data.nodeId}`);
  testRunner.completeTest();
})
