(async function(testRunner) {
  // Test traces
  var {session} = await testRunner.startHTML(
      `
      <head></head>
      <body>
      </body>
  `,
      'Tests reporting of long animation frames in traces (no visual update).');

  var TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  await session.evaluate(`
    setTimeout(() => {
      const busy_wait_time = 250;
      const deadline = performance.now() + busy_wait_time;
      while (performance.now() < deadline) {}
    })`);
  await new Promise(r => setTimeout(r, 500));
  const events = await tracingHelper.stopTracing();
  const loaf_tracing_event = events.find(
      e => e.name == 'LongAnimationFrame' && e.args.data.duration > 200);
  testRunner.log(
      loaf_tracing_event ? 'Found matching LoAF event' :
                           'No matching LoAF event')
  if (loaf_tracing_event) {
    const {data} = loaf_tracing_event.args;
    testRunner.log(`duration-blockingDuration${
        data.duration - data.blockingDuration >= 50 ? '>=50' : '<50'}`)
    testRunner.log(`numScripts=${data.numScripts}`);
  }
  testRunner.completeTest();
})
