(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of image decoding trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  const Phase = TracingHelper.Phase;
  await dp.Page.enable();
  await tracingHelper.startTracing('disabled-by-default-devtools.timeline');
  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/resources/rendering-exercise.html'
  });

  // Wait for trace events.
  await dp.Page.onceLoadEventFired();
  await session.evaluateAsync(`window.document.querySelector('img').decode()`);

  await tracingHelper.stopTracing(/(disabled-by-default-)?devtools\.timeline/);

  const decodeImage = tracingHelper.findEvent(
      'Decode Image', Phase.COMPLETE);
  const drawLazyPixelRef = tracingHelper.findEvent(
      'Draw LazyPixelRef', Phase.INSTANT);
  const decodeLazyPixelRef = tracingHelper.findEvent(
      'Decode LazyPixelRef', Phase.COMPLETE);


  testRunner.log('Got a Decode Image event:');
  tracingHelper.logEventShape(drawLazyPixelRef);

  testRunner.log('Got an Draw LazyPixelRef event');
  tracingHelper.logEventShape(decodeImage);

  testRunner.log('Got a Decode LazyPixelRef Event');
  tracingHelper.logEventShape(decodeLazyPixelRef);

  testRunner.completeTest();
})
