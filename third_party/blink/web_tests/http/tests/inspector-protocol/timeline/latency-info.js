(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of LatencyInfo.Flow and InputLatency::* trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  // Start tracing and dispatch two input events.
  await tracingHelper.startTracing('benchmark,latencyInfo,rail');
  await dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

  await dp.Input.dispatchKeyEvent({
    type: 'keyDown',
  });
  await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
  });

  // Wait for trace events.
  await new Promise(resolve => setTimeout(resolve, 1000))

  // Filter trace events to input LatencyInfo.flow and InputLatency::* only.
  const devtoolsEvents = await tracingHelper.stopTracing(
      /(input,)?benchmark,(latencyInfo,rail|devtools.timeline)/);

  const flowEvents =
      devtoolsEvents.filter(event => event.name === 'LatencyInfo.Flow');
  const inputEvents = devtoolsEvents.filter(
      event => /InputLatency::(KeyDown|KeyUp)/.test(event.name));

  if (!flowEvents.length || !inputEvents) {
    testRunner.log('FAIL: did not receive any input events');
    testRunner.completeTest();
    return;
  }

  const knownInputEvents = new Set();

  // Test the data of LatencyInfo.Flow events.
  for (const flowEvent of flowEvents) {
    if (!flowEvent.args.chrome_latency_info) {
      continue;
    }
    const traceId = flowEvent.args.chrome_latency_info.trace_id;
    const traceIdType = typeof traceId;
    if (traceIdType !== 'number') {
      testRunner.log(`Found unexpected type for trace_id: ${traceIdType}`);
      testRunner.completeTest();
    }
    knownInputEvents.add(traceId);
  }

  testRunner.log('Validated LatencyInfo.Flow events.');

  const openInputEvents = new Set();
  // Test the data of InputLatency::* events.
  for (const inputEvent of inputEvents) {
    if (inputEvent.ph === 'e') {
      openInputEvents.delete(inputEvent.id);
      continue;
    }
    testRunner.log(`Input event name: ${inputEvent.name}`);
    testRunner.log(`Input event phase: ${inputEvent.ph}`);

    const latencyInfo = inputEvent.args.chrome_latency_info;
    const traceId = latencyInfo.trace_id;
    // Make sure there is a corresponding LatencyInfo.Flow for each
    // input event.
    if (!knownInputEvents.has(traceId)) {
      testRunner.log(`Found unexpected trace_id: ${inputEvent.ph}`);
      testRunner.completeTest();
    }
    openInputEvents.add(inputEvent.id);

    const causedFrame = latencyInfo.component_info.some(
        (c) => c['component_type'] ===
            'COMPONENT_INPUT_EVENT_LATENCY_RENDERER_SWAP');
    testRunner.log(`Input event caused frame: ${causedFrame}`);
  }
  // Test that for each 'begin' event, we received a corresponding
  // 'end' event.
  if (openInputEvents.size > 0) {
    testRunner.log(`Did not find closing events for: ${[...openInputEvents]}`);
  }
  testRunner.completeTest();
})
