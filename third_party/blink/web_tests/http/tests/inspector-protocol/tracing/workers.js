(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of worker trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing(
      '__metadata,disabled-by-default-devtools.timeline');

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
      /__metadata|disabled-by-default-devtools.timeline/);

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

  testRunner.completeTest();
});
