(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} =
      await testRunner.startBlank('Tests the data of iframe post trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('devtools.timeline');

  // Wait for trace events.
  await session.evaluateAsync(`
    (function(){
        const frame = document.createElement('iframe');
        document.body.appendChild(frame);
        frame.contentWindow.postMessage('Ping!', '*');

        return new Promise((resolve) => {
            window.onmessage = (e) => resolve(e.data);
            frame.contentWindow.onmessage = (e) => {
                window.postMessage('Pong!', '*');
            };
        });
    })();
  `);

  await tracingHelper.stopTracing(/devtools\.timeline/);

  let matchingScheduleTraceIds = new Set();
  const schedulePostMessageEvents =
      tracingHelper.findEvents('SchedulePostMessage', Phase.INSTANT);
  testRunner.log(`Found ${
      schedulePostMessageEvents.length} SchedulePostMessage events in total`);

  schedulePostMessageEvents?.forEach(event => {
    testRunner.log('Got SchedulePostMessage event:');
    matchingScheduleTraceIds.add(event.args?.data?.traceId);
    tracingHelper.logEventShape(event.args?.data);
  });

  const handlePostMessageEvents =
      tracingHelper.findEvents('HandlePostMessage', Phase.COMPLETE);
  testRunner.log(`Found ${
      handlePostMessageEvents.length} HandlePostMessage events in total`);

  let matchingHandlerTraceIds = new Set();
  handlePostMessageEvents?.forEach(event => {
    testRunner.log('Got HandlePostMessage event:');
    matchingHandlerTraceIds.add(event.args?.data?.traceId);
    tracingHelper.logEventShape(event.args?.data);
  });

  if (matchingHandlerTraceIds.size  !== matchingScheduleTraceIds.size) {
    testRunner.log(`Set containing Trace Ids for HandlePostMessage and SchedulePostMessage events should be the same size`);
  }

  matchingScheduleTraceIds.forEach(traceId => {
    if (matchingHandlerTraceIds.has(traceId)) {
      testRunner.log(
          'SchedulePostMessage and HandlePostMessage trace events are correctly linked');
    } else {
      testRunner.log(`Non-matching id: ${traceId} - SchedulePostMessage and HandlePostMessage trace events are incorrectly linked`);
    }
  });
  testRunner.completeTest();
});
