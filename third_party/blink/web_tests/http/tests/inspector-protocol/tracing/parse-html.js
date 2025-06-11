(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the ParseHTML trace events and the line numbers');
  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await dp.Page.enable();
  await tracingHelper.startTracing('devtools.timeline');
  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/resources/parse-html.html'
  });
  await dp.Page.onceLoadEventFired();
  const timelineEvents = await tracingHelper.stopTracing(/devtools.timeline/);
  const parseHTMLEvents = timelineEvents.filter(e => e.name === 'ParseHTML');
  const eventAssertions = [];
  for (const event of parseHTMLEvents) {
    eventAssertions.push({
      startLine: event.args.beginData.startLine,
      endLine: event.args.endData.endLine,
    });
  }
  // Sometimes we can get more than 2 parseHTML events; but we only care about the first two for this test.
  eventAssertions.splice(2);
  eventAssertions.forEach((data, index) => {
    testRunner.log(`ParseHTML Number ${index}: startLine=${data.startLine}, endLine=${data.endLine}.`);
  });
  testRunner.completeTest();
});

