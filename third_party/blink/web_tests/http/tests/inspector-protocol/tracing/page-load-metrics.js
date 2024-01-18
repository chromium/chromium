(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of page load and web vitals trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline,devtools.timeline,loading');
  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/web-vitals.html'
  });

  // Wait for trace events.
  await session.evaluateAsync(`
    new Promise((res) => {
      (new PerformanceObserver(res)).observe({entryTypes: ['layout-shift']});
    })`);

  await tracingHelper.stopTracing(
      /loading|(disabled-by-default-)?devtools.timeline/);

  // Web vitals
  const firstContentfulPaint =
      tracingHelper.findEvent('firstContentfulPaint', Phase.MARK);
  const largestContentfulPaintCandidate =
      tracingHelper.findEvents('largestContentfulPaint::Candidate', Phase.MARK).find(candidate => candidate.args.data.navigationId === firstContentfulPaint.args.data.navigationId);
  const layoutShift = tracingHelper.findEvent('LayoutShift', Phase.INSTANT);


  testRunner.log('\nGot FCP event:');
  tracingHelper.logEventShape(firstContentfulPaint);

  testRunner.log('\nGot LCP candidate event:');
  tracingHelper.logEventShape(largestContentfulPaintCandidate);

  testRunner.log('\nGot LayoutShift event:');
  tracingHelper.logEventShape(layoutShift);

  // "Marker" events
  const firstPaint = tracingHelper.findEvent('firstPaint', Phase.MARK);
  const markDOMContent = tracingHelper.findEvent('MarkDOMContent', Phase.INSTANT);
  const markLoad = tracingHelper.findEvent('MarkLoad', Phase.INSTANT);

  testRunner.log('\nGot firstPaint event:');
  tracingHelper.logEventShape(firstPaint);

  testRunner.log('\nGot MarkDOMContent event:');
  tracingHelper.logEventShape(markDOMContent);

  testRunner.log('\nGot MarkLoad event:');
  tracingHelper.logEventShape(markLoad)

  // Long tasks
  const runTask = tracingHelper.findEvent('RunTask', Phase.COMPLETE);

  testRunner.log('\nGot RunTask event:');
  tracingHelper.logEventShape(runTask)

  const networkRequest = tracingHelper.findEvent('ResourceSendRequest', Phase.INSTANT);
  const navigationId = networkRequest.args.data.requestId;

  const allRequestIds = [
    largestContentfulPaintCandidate,
    firstContentfulPaint,
    firstPaint,
  ].map(event => event.args.data.navigationId);


  const allIdsAreEqual = allRequestIds.every(id => id === navigationId);

  if (allIdsAreEqual) {
    testRunner.log('\nPage load events belong to the same navigation.');
  }

  testRunner.completeTest();
})
