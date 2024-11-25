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
        <span id="output"></span>
      </body>
  `,
      'Test that long animation frame does not overlap with paint time');

  var TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  await session.evaluateAsync(`new Promise(resolve => {
    requestAnimationFrame(() => {
      const busy_wait_time = 120;
      const deadline = performance.now() + busy_wait_time;
      while (performance.now() < deadline) {}
      document.querySelector("#output").innerHTML = performance.now();
      resolve();
    });
  });`);
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(() => resolve()));`);
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(() => resolve()));`);
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(() => resolve()));`);
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(() => resolve()));`);
  await session.evaluateAsync(`new Promise(resolve => setTimeout(() => resolve(), 500));`);
  const events = await tracingHelper.stopTracing();
  const loaf_events = events.filter(e => e.name === "AnimationFrame" && e.args.animation_frame_timing_info);
  if (!loaf_events) {
    testRunner.log("No AnimationFrame events");
    testRunner.completeTest();
    return;
  }

  let found_paint_event = false;
  for (const loaf_tracing_event of loaf_events) {
    const start_time = loaf_tracing_event.ts;
    const loaf_end_time = start_time + loaf_tracing_event.args.animation_frame_timing_info.duration_ms;
    const presentation_time = events.find(e => e.name === "AnimationFrame::Presentation" && e.args.id === loaf_tracing_event.id)?.ts ?? 0;

    const paint_event = events.filter(e => (e.name === "Paint" || e.name === "Layerize") && e.ts > start_time && e.ts < presentation_time);
    if (!paint_event)
      continue;
    if (!found_paint_event)
        testRunner.log("Found paint event");
    found_paint_event = true;
    if (paint_event.ts <= loaf_end_time) {
      testRunner.log("Found paint events that overlaps with AnimationFrame");
      testRunner.completeTest();
    }
  }
  testRunner.log("Paint events do not overlap with AnimationFrame");
  testRunner.completeTest();
})
