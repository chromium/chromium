(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of EventTiming trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('devtools.timeline');

  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/interactions.html'
  });

  // Wait for the DOM to be interactive.
  await session.evaluateAsync(`new Promise((resolve) => onload = resolve)`);

  // Dispatch a keyboard interaction.
  await dp.Input.dispatchKeyEvent({type: 'keyDown', key: 'A'});
  await dp.Input.dispatchKeyEvent({type: 'keyUp', key: 'A'});

  // Wait for trace events and stop tracing.
  await session.evaluateAsync(`window.__interactionPromise`);
  const devtoolsEvents = await tracingHelper.stopTracing(/devtools\.timeline/);

  const eventTimingTraces =
      devtoolsEvents.filter(event => event.name === 'EventTiming');
  const keyUpBeginEvent =
      eventTimingTraces.find(event => event.args?.data?.type === 'keydown');
  const keyUpEndEvent = eventTimingTraces.find(
      event => event.id === keyUpBeginEvent.id && !event.args.data);
  testRunner.log(`Got EventTiming begin event for keydown event with phase ${
      keyUpBeginEvent.ph}:`);
  tracingHelper.logEventShape(keyUpBeginEvent);
  testRunner.log(`Got EventTiming end event for keydown event with phase ${
      keyUpEndEvent.ph}:`);
  tracingHelper.logEventShape(keyUpEndEvent);
  testRunner.completeTest();
})
