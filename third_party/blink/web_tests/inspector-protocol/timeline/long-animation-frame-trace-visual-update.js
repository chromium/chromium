(async function(testRunner) {
  // Test traces
  var {session} = await testRunner.startHTML(
      `
      <head></head>
      <body>
      </body>
  `,
      'Tests reporting of long animation frames in traces (with visual update).');

  var TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  for (let retries = 0; retries < 20; ++retries) {
    await session.evaluate(`
    requestAnimationFrame(() => {
      const busy_wait_time = 250;
      const deadline = performance.now() + busy_wait_time;
      while (performance.now() < deadline) {}
    })`);
    await new Promise(r => setTimeout(r, 500));
    const events = await tracingHelper.stopTracing();
    const loaf_tracing_event = events.find(
        e => e.name == 'LongAnimationFrame' && e.args.data.duration > 200 &&
            e.args.data.renderDuration > 200);
    if (!loaf_tracing_event)
      continue;
    testRunner.log(
        loaf_tracing_event ? 'Found matching LoAF event' :
                             'No matching LoAF event');
    if (loaf_tracing_event) {
      const {data} = loaf_tracing_event.args;
      testRunner.log(`duration-blockingDuration${
          data.duration - data.blockingDuration >= 50 ? '>=50' : '<50'}`)
      testRunner.log(
          `renderDuration${data.renderDuration > 0 ? '>0' : ' not found'}`);
      testRunner.log(`styleAndLayoutDuration${
          data.styleAndLayoutDuration > 0 ? '>0' : ' not found'}`);
      testRunner.log(`numScripts=${data.numScripts}`);
    }

    break;
  }
  testRunner.completeTest();
})
