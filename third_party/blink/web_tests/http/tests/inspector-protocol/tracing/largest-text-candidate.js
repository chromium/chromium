(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the LargestTextPaint::candiate trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Network.enable();

  await tracingHelper.startTracing('loading');

  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/text-lcp.html'});
  await session.evaluateAsync(`
    new Promise((res) => {
      new PerformanceObserver(function() {
          setTimeout(res, 100);
      }).observe({entryTypes: ['largest-contentful-paint']});
    })`);
  const events = await tracingHelper.stopTracing(/loading/);
  const textCandidates = events.filter(e => e.name === "LargestTextPaint::Candidate")
  if(textCandidates.length < 1) {
    testRunner.log(`FAILED: no LargestTextPaint::Candiate events found`);
    testRunner.completeTest();
    return;
  }

  const candidate = textCandidates[0];
  testRunner.log('Event shape:')
  tracingHelper.logEventShape(candidate);
  testRunner.log(`nodeName: ${candidate.args.data.nodeName}`);
  testRunner.completeTest();
});
