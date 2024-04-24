(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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
    await session.evaluateAsync(`new Promise(resolve => {
      requestAnimationFrame(() => {
        const busy_wait_time = 250;
        const deadline = performance.now() + busy_wait_time;
        while (performance.now() < deadline) {}
        resolve();
      });
    });`);
    // Ensure at least 4 "short" animation frames occur.
    await session.evaluateAsync(`new Promise(r => requestAnimationFrame(r));`);
    await session.evaluateAsync(`new Promise(r => requestAnimationFrame(r));`);
    await session.evaluateAsync(`new Promise(r => requestAnimationFrame(r));`);
    await session.evaluateAsync(`new Promise(r => requestAnimationFrame(r));`);
    const events = await tracingHelper.stopTracing();
    const loaf_tracing_event = events.find(
        e => e.name == 'AnimationFrame' &&
            e.args.data?.duration > 200 &&
            e.args.data?.renderDuration > 200 &&
            e.args.data?.numScripts == 1);
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
    const short_animation_frame_events = events.filter(
      e => e.name == 'AnimationFrame' && e.args.data?.numScripts == 0);
    testRunner.log(`Found matching short LoaF events ${
        short_animation_frame_events.length >= 4 ? '>=4' : '<4'}`);

    break;
  }
  testRunner.completeTest();
})
