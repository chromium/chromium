(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of page load and web vitals trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline,disabled-by-default-devtools.timeline.frame,devtools.timeline,loading');
  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/web-vitals.html'
  });

  // Wait for trace events.
  const largestContentfulPaintPromise = await session.evaluateAsync(`
      (function() {
        let resolvePromiseCallback = () => ({});
        const observerCallBack = entryList => {
          const entries = entryList.getEntries();
          for (const entry of entries) {
            if (entry.element.tagName === 'IMG' && entry.element.getAttribute('src', './big-image.png')) {
              resolvePromiseCallback();
            }
          }
        };
        const observer = new PerformanceObserver(observerCallBack);
        const largestContentfulPaintPromise = new Promise((res) => {resolvePromiseCallback = res})
        observer.observe({entryTypes: ['largest-contentful-paint']});
        return largestContentfulPaintPromise;
      })()
    `);
  await largestContentfulPaintPromise;
  await tracingHelper.stopTracing(
      /loading|(disabled-by-default)?devtools.timeline(.frame)?|rail/);

  const drawFrame = tracingHelper.findEvents('DrawFrame', 'I').at(-1);

  function hasExpectedFrameSeqId(event) {
    return event.args.frameSeqId === drawFrame.args.frameSeqId;
  }
  function hasExpectedLayerTreeId(event) {
    return event.args.layerTreeId === drawFrame.args.layerTreeId;
  }

  // Given a DrawFrame event, find the events marking the other steps
  // of that frame's life cycle.

  const beginFrame =
      tracingHelper.findEvent('BeginFrame', Phase.INSTANT, hasExpectedFrameSeqId);
  const commit =
      tracingHelper.findEvent('Commit', Phase.COMPLETE, hasExpectedFrameSeqId);
  const activateLayerTree =
      tracingHelper.findEvent('ActivateLayerTree', Phase.INSTANT, hasExpectedLayerTreeId);
  const needsBeginFrameChanged = tracingHelper.findEvent(
      'NeedsBeginFrameChanged', Phase.INSTANT, hasExpectedLayerTreeId);
  const beginMainThreadFrame = tracingHelper.findEvent(
      'BeginMainThreadFrame', Phase.INSTANT, hasExpectedLayerTreeId);

  // Other trace events in the frame domain that do not necessarily
  // belong to the life cycle of the DrawFrame event used above.
 const setLayerTreeId = tracingHelper.findEvent('SetLayerTreeId', Phase.INSTANT);
  const paint = tracingHelper.findEvent('Paint', Phase.COMPLETE);
  const screenshots = tracingHelper.findEvents('Screenshot', Phase.SNAPSHOT_OBJECT);

  testRunner.log('Got SetLayerTreeId event:');
  tracingHelper.logEventShape(setLayerTreeId)

  testRunner.log('Got BeginFrame event:');
  tracingHelper.logEventShape(beginFrame)

  testRunner.log('Got DrawFrame event:');
  tracingHelper.logEventShape(drawFrame)

  testRunner.log('Got ActivateLayerTree event:');
  tracingHelper.logEventShape(activateLayerTree)

  testRunner.log('Got NeedsBeginFrameChanged event:');
  tracingHelper.logEventShape(needsBeginFrameChanged)

  testRunner.log('Got BeginMainThreadFrame event:');
  tracingHelper.logEventShape(beginMainThreadFrame)

  testRunner.log('Got Paint event:');
  tracingHelper.logEventShape(paint)

  testRunner.log('Got Commit event:');
  tracingHelper.logEventShape(commit);

  if (screenshots && screenshots.every(s => s.args.snapshot)) {
    testRunner.log('All screenshots have image data.');
  }

  testRunner.completeTest();
})
