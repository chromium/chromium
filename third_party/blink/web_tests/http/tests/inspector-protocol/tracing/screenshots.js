(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the generation of screenshot events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await tracingHelper.startTracing('disabled-by-default-devtools.screenshot');

  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/web-vitals.html'
  });

  await dp.Page.onceLoadEventFired();
  // Wait for trace events, and then some time for screenshots.
  // TODO(crbug.com/391786494) I have no idea why, but a 2s timeout is the only
  // way I can reliably get the screenshot trace events to appear.

  await session.evaluateAsync(`
    new Promise((res) => {
      new PerformanceObserver(function() {
          setTimeout(res, 2000);
      }).observe({entryTypes: ['largest-contentful-paint']});
    })`);

  const screenshots = await tracingHelper.stopTracing(/screenshot/);
  if(screenshots.length < 1) {
    testRunner.log('ERROR: did not get any screenshot events');
    testRunner.completeTest();
  }

  tracingHelper.logEventShape(screenshots[0]);
  testRunner.completeTest();
});
