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

  const loadEvent = tracingHelper.findEvent(
    'CommitLoad', Phase.COMPLETE,
    evt => evt.args.data.url.endsWith('/web-vitals.html'));
  const rendererPid = loadEvent.pid;
  function matchesRendererPid(evt) {
    return evt.pid === rendererPid;
  }

  const setLayerTreeId = tracingHelper.findEvents(
    'SetLayerTreeId', Phase.INSTANT, matchesRendererPid).at(-1);
  const layerTreeId = setLayerTreeId.args.data.layerTreeId;
  function matchesLayerTree(evt) {
    return matchesRendererPid(evt) && evt.args.layerTreeId === layerTreeId;
  }

  const beginMainThreadFrame = tracingHelper.findEvents(
    'BeginMainThreadFrame', Phase.INSTANT, matchesLayerTree).at(-1);
  const frameId = beginMainThreadFrame.args.data.frameId;
  function matchesFrameId(evt) {
    return matchesLayerTree(evt) && evt.args.frameId === frameId;
  }

  const drawFrame = tracingHelper.findEvents(
    'DrawFrame', Phase.INSTANT, matchesLayerTree).at(-1);
  const frameSeqId = drawFrame.args.frameSeqId;
  function matchesFrameSeqId(evt) {
    return matchesLayerTree(evt) && evt.args.frameSeqId === frameSeqId;
  }


  const beginFrame = tracingHelper.findEvent(
    'BeginFrame', Phase.INSTANT, matchesFrameSeqId);
  const commit = tracingHelper.findEvent(
    'Commit', Phase.COMPLETE, matchesFrameSeqId);
  const activateLayerTree = tracingHelper.findEvent(
    'ActivateLayerTree', Phase.INSTANT, matchesFrameId);

  // Other trace events in the frame domain that do not necessarily
  // belong to the life cycle of the DrawFrame event used above.
  const needsBeginFrameChanged = tracingHelper.findEvent(
    'NeedsBeginFrameChanged', Phase.INSTANT);
  const paint = tracingHelper.findEvent(
    'Paint', Phase.COMPLETE, matchesRendererPid);
  const screenshots = tracingHelper.findEvents(
    'Screenshot', Phase.SNAPSHOT_OBJECT);

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
