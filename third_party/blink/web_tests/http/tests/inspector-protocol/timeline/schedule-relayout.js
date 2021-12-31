(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that a trace event is dispatched at LayoutObject::ScheduleRelayout');

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('loading');

  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/relayout.html'});

  // Wait for trace events.
  await new Promise(resolve => setTimeout(resolve, 1000))

  const devtoolsEvents = await tracingHelper.stopTracing(/loading/);
  const scheduleRelayoutEvents = devtoolsEvents.filter(event => event.name === 'LayoutObject::ScheduleRelayout');
  if (!scheduleRelayoutEvents.length) {
    testRunner.log('FAIL: did not receive any LayoutObject::ScheduleRelayout event');
    testRunner.completeTest();
    return;
  }
  const scheduleRelayoutEvent = scheduleRelayoutEvents[0];
  testRunner.log('Trace event received');
  testRunner.log(`Event name: ${scheduleRelayoutEvent.name}`);
  testRunner.log(`Type of event frame: ${typeof scheduleRelayoutEvent.args.frame}`);
  testRunner.log(`Type of event nodeId: ${typeof scheduleRelayoutEvent.args.data.nodeId}`);
  testRunner.completeTest();
})
