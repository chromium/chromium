(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the "LinkPreconnect" trace event');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Network.enable();

  await tracingHelper.startTracing('disabled-by-default-devtools.timeline');

  // Load the page and wait for LCP, which means the page is loaded and we can stop tracing.
  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/link-preconnect.html'});
  await session.evaluateAsync(`
    new Promise((res) => {
      new PerformanceObserver(function() {
          setTimeout(res, 100);
      }).observe({entryTypes: ['largest-contentful-paint']});
    })`);
  const events = await tracingHelper.stopTracing(/devtools\.timeline/);
  const preconnects = events.filter(e => e.name === "LinkPreconnect")
  if(preconnects.length < 1) {
    testRunner.log(`FAILED: no LinkPreconnect events found`);
    testRunner.completeTest();
    return;
  }
  testRunner.log(`Count of LinkPreconnect events found: ${preconnects.length}`);

  const preconnect = preconnects[0];
  testRunner.log('\nEvent shape:');
  tracingHelper.logEventShape(preconnect);
  testRunner.log(`Preconnect URL: ${preconnect.args.data.url}`);
  testRunner.completeTest();
});
