(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of worker trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing(
      '__metadata,disabled-by-default-devtools.timeline,devtools.timeline');

  // Wait for trace events.
  await session.evaluateAsync(`
    (function(){
        let callBack = () => ({});
        const promise = new Promise((resolve) => callBack = resolve);
        const worker = new Worker("../resources/worker.js");
        worker.onmessage = callBack;
        worker.postMessage("");
        return promise;
    })();
    `);
  await tracingHelper.stopTracing(
      /__metadata|disabled-by-default-devtools.timeline|devtools.timeline/);

  const tracingSessionIdForWorker =
      tracingHelper.findEvent('TracingSessionIdForWorker', Phase.INSTANT);

  testRunner.log('Got TracingSessionIdForWorker event:');
  tracingHelper.logEventShape(tracingSessionIdForWorker);

  const threadNames = tracingHelper.findEvents('thread_name', Phase.METADATA);

  const workerThread =
      threadNames.find(event => event.args.name === 'DedicatedWorker thread');

  testRunner.log('Got Worker thread name event:');
  tracingHelper.logEventShape(workerThread);

  if (tracingSessionIdForWorker.args.data.workerThreadId === workerThread.tid) {
    testRunner.log('Data was found for worker.');
  }

  let matchingScheduleTraceIds = new Set();
  const schedulePostMessageEvents =
      tracingHelper.findEvents('SchedulePostMessage', Phase.INSTANT);
  testRunner.log(`Found ${
      schedulePostMessageEvents.length} SchedulePostMessage events in total`);

  schedulePostMessageEvents?.forEach(event => {
    testRunner.log('Got SchedulePostMessage event:');
    tracingHelper.logEventShape(event.args?.data);
    matchingScheduleTraceIds.add(event.args?.data?.traceId);
  });

  const handlePostMessageEvents =
      tracingHelper.findEvents('HandlePostMessage', Phase.COMPLETE);
  testRunner.log(`Found ${
      handlePostMessageEvents.length} HandlePostMessage events in total`);

  let matchingHandlerTraceIds = new Set();
  handlePostMessageEvents?.forEach(event => {
    testRunner.log('Got HandlePostMessage event:');
    tracingHelper.logEventShape(event.args?.data);
    matchingHandlerTraceIds.add(event.args?.data?.traceId);
  });

  if (matchingHandlerTraceIds.size  !== matchingScheduleTraceIds.size) {
    testRunner.log(`Set containing Trace Ids for HandlePostMessage and SchedulePostMessage events should be the same length`);
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
