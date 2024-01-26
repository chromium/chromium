(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of EventTiming trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  const Phase = TracingHelper.Phase;

  await dp.Page.enable();
  await tracingHelper.startTracing('devtools.timeline');

  dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/interactions.html'
  });

  // Wait for the DOM to be interactive.
  await dp.Page.onceLoadEventFired();

  // Dispatch a keyboard interaction.
  await dp.Input.dispatchKeyEvent({type: 'keyDown', key: 'A'});
  await dp.Input.dispatchKeyEvent({type: 'keyUp', key: 'A'});

  await dp.Input.dispatchKeyEvent({type: 'keyDown', key: 'B'});
  await dp.Input.dispatchKeyEvent({type: 'keyUp', key: 'B'});

  await dp.Input.dispatchKeyEvent({type: 'keyDown', key: 'C'});
  await dp.Input.dispatchKeyEvent({type: 'keyUp', key: 'C'});

  // Wait for trace events and stop tracing.
  await session.evaluateAsync(`window.__interactionPromise`)
  const devtoolsEvents = await tracingHelper.stopTracing(/devtools\.timeline/);

  const eventTimingTraces =
      devtoolsEvents.filter(event => event.name === 'EventTiming');

  const eventDispatch = tracingHelper.findEvent('EventDispatch', Phase.COMPLETE);

  const keyBeginEvent = eventTimingTraces.find(
      event => event.args?.data?.type === 'keydown' ||
          event.args?.data?.type === 'keyup');
  const keyEndEvent = eventTimingTraces.find(
      event => event.id === keyBeginEvent.id && !event.args.data);
  testRunner.log(`Got EventTiming begin event for keydown event with phase ${
      keyBeginEvent.ph}:`);
  tracingHelper.logEventShape(keyBeginEvent);
  testRunner.log(`Got EventTiming end event for keydown event with phase ${
      keyEndEvent.ph}:`);
  tracingHelper.logEventShape(keyEndEvent);

  testRunner.log('Got EventDispatch event');
  tracingHelper.logEventShape(eventDispatch);

  testRunner.completeTest();
});
