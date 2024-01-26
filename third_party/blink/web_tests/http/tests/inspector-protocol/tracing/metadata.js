(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of metadata trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing(
      '__metadata,loading,blink.user_timing,disabled-by-default-devtools.timeline,devtools.timeline');

  await dp.Page.enable();

  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/resources/iframe-navigation.html'
  });

  // Wait for trace events.
  await dp.Page.onceLoadEventFired();

  await tracingHelper.stopTracing(
      /__metadata|loading|blink.user_timing|(disabled-by-default-)?devtools.timeline/);

  // Processes and threads
  const processNames = tracingHelper.findEvents('process_name', Phase.METADATA);
  const threadNames = tracingHelper.findEvents('thread_name', Phase.METADATA);

  const browserProcessName =
      processNames.find(event => event.args.name === 'Browser');
  const gpuProcessName = processNames.find(event => event.args.name === 'GPU Process');
  const rendererProcessNames =
      processNames.filter(event => event.args.name === 'Renderer');

  const gpuThread = threadNames.find(event => event.args.name === 'CrGpuMain');
  const browserThread =
      threadNames.find(event => event.args.name === 'CrBrowserMain');

  // Frames in trace
  const tracingStartedInBrowser =
      tracingHelper.findEvent('TracingStartedInBrowser', Phase.INSTANT);
  const frameCommittedInBrowserEvents =
      tracingHelper.findEvents('FrameCommittedInBrowser', Phase.INSTANT);
  const commitLoadEvents = tracingHelper.findEvents('CommitLoad', Phase.COMPLETE);

  // Other
  const navigarionStart = tracingHelper.findEvent('navigationStart', Phase.MARK);
  const viewPort =
      tracingHelper.findEvent('PaintTimingVisualizer::Viewport', Phase.INSTANT);

  // Extract all frames in target from trace.
  const initialFrames =
      tracingStartedInBrowser.args.data.frames.map(frame => frame.frame);
  const commitedFrames = [
    ...frameCommittedInBrowserEvents, ...commitLoadEvents
  ].map(event => event.args.data.frame);
  const allFramesInTrace = new Set([...initialFrames, ...commitedFrames]);


  testRunner.log('Got TracingStartedInBrowser event:');
  tracingHelper.logEventShape(tracingStartedInBrowser);

  testRunner.log('Got Browser process name event:');
  tracingHelper.logEventShape(browserProcessName);

  testRunner.log('Got GPU process name event:');
  tracingHelper.logEventShape(gpuProcessName);

  testRunner.log('Got Renderer process name event:');
  tracingHelper.logEventShape(rendererProcessNames[0]);

  testRunner.log('Got GPU thread name event:');
  tracingHelper.logEventShape(gpuThread);

  testRunner.log('Got Browser thread name event:');
  tracingHelper.logEventShape(browserThread);

  testRunner.log('Got navigarionStart event:');
  tracingHelper.logEventShape(navigarionStart);

  testRunner.log('Got Viewport event:');
  tracingHelper.logEventShape(viewPort);

  testRunner.log(`Found ${allFramesInTrace.size} unique frames`);

  testRunner.completeTest();
});
