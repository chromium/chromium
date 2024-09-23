(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // Test traces
  function checkAsyncEventDuration(name, events, duration, expected_id) {
    const eventList = events.filter(e => e.name == name);
    if (eventList.length < 2) {
      testRunner.log(`${name} trace events not emitted`);
    } else if ((eventList[1].ts - eventList[0].ts) / 1000 <= duration) {
      testRunner.log(`${name} duration <= ${duration}`);
    } else {
      testRunner.log(`${name} duration > ${duration}`);
    }
    testRunner.log(`${name}.id ${
        eventList[0].id == expected_id && eventList[1].id == expected_id ? '==' : '!='
        } AnimationFrame.id`);
  }
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
            e.args.animation_frame_timing_info?.duration_ms > 200 &&
            e.args.animation_frame_timing_info?.num_scripts == 1);
    if (!loaf_tracing_event)
      continue;
    testRunner.log(
        loaf_tracing_event ? 'Found matching LoAF event' :
                             'No matching LoAF event');
    if (loaf_tracing_event) {
      const {animation_frame_timing_info} = loaf_tracing_event.args;
      testRunner.log(`duration-blockingDuration${
        animation_frame_timing_info.duration_ms -
            animation_frame_timing_info.blocking_duration_ms >= 50 ? '>=50' : '<50'}`)
      testRunner.log(`numScripts=${animation_frame_timing_info.num_scripts}`);
    }
    const id = loaf_tracing_event.id;
    checkAsyncEventDuration('AnimationFrame::Render', events, 200, id);
    checkAsyncEventDuration('AnimationFrame::StyleAndLayout', events, 0, id);
    checkAsyncEventDuration('AnimationFrame::Script::Execute', events, 200, id);
    const scriptEvent = events.find(e => e.name == 'AnimationFrame::Script::Execute');
    testRunner.log(`AnimationFrame::Script::Execute property_like_name ${
        scriptEvent.args.animation_frame_script_timing_info.property_like_name}`);
    testRunner.log(`AnimationFrame::Script::Execute invoker_type ${
        scriptEvent.args.animation_frame_script_timing_info.invoker_type}`);
    // We want to assert that there are exactly four short AnimationFrame events with
    // no scripts counted against. However, on some really slow devices (eg. cpu=1 tests),
    // this tends to flake with short frames' script execution time going above the 5ms
    // threshold. In order to deflake, we use a < 200 threshold here to count short frames.
    const short_animation_frame_events = events.filter(
        e => e.name == 'AnimationFrame' &&
          (e.args.animation_frame_timing_info?.duration_ms || 0) < 200);
    testRunner.log(`Found matching short LoaF events ${
        short_animation_frame_events.length >= 4 ? '>=4' : '<4'}`);
    const short_frame_ids = short_animation_frame_events.map(({ id }) => id);
    testRunner.log(`Found matching short AnimationFrame events ${
        short_animation_frame_events.length >= 4 ? '>=4' : '<4'}`);
    const short_animation_frame_render_events = events.filter(
        e => e.name == 'AnimationFrame::Render' && short_frame_ids.includes(e.id));
    testRunner.log(`Found matching short AnimationFrame::Render events ${
        short_animation_frame_render_events.length >= 4 ? '>=4' : '<4'}`);

    break;
  }
  testRunner.completeTest();
})
