(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that RuntimeCallStats are present in profile');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('v8,devtools.timeline,devtools.timeline.frame,disabled-by-default-v8.runtime_stats_sampling,disabled-by-default-v8.cpu_profiler,disabled-by-default-v8.compile');
  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/runtime-stats.html'
  });

  await session.evaluateAsync(`
      (function() {
          var div = document.getElementById("foo")
          div.style.width = "10px";
          return div.offsetWidth;
      })()
    `);
    // A small amount of time to let the trace events gather.
  await new Promise(r => setTimeout(r, 100))
  await tracingHelper.stopTracing(/.*/);

  const profile = tracingHelper.findEvents('Profile', 'P');
  const profileChunks = tracingHelper.findEvents('ProfileChunk', 'P');

  // Assert that we found at least one profile chunk with one callFrame node
  // that is native.
  const foundNativeCallFrame = profileChunks.some(chunk => {
    return chunk.args?.data?.cpuProfile?.nodes?.some(node => {
        return node.callFrame?.url === 'native V8Runtime'
    })
  })

  testRunner.log(`Found native V8Runtime callFrame: ${foundNativeCallFrame}`)

  testRunner.completeTest();
})
