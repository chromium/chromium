(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Network request lifecycle trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Network.enable();

  await tracingHelper.startTracing('devtools.timeline,disabled-by-default-devtools.timeline.stack');

  // Get the ID of the request for the HTML page.
  // Kick this off before we navigate to ensure the navigation is not completed before this listener is added.
  const htmlRequestPromise = dp.Network.onceRequestWillBeSent(e => {
    return e.params.request.url.includes('basic.html');
  });

  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

  const htmlRequest = await htmlRequestPromise;

  // Wait for the HTML request so we can assert on the trace events.
  // Note: we used to wait for all 4 requests in the URL above to complete, but
  // it caused regular flakes and timeouts across bots.
  // See crbug.com/40268741 for the various attempts to make this test stable.
  await dp.Network.onceLoadingFinished(e => {
    return e.params.requestId === htmlRequest.params.requestId;
  });

  const timelineEvents = await tracingHelper.stopTracing(/devtools.timeline/);
  const eventNames = new Set(['ResourceSendRequest', 'ResourceWillSendRequest', 'ResourceReceiveResponse', 'ResourceReceivedData', 'ResourceFinish']);

  const matchingEventsForRequest = []
  for (const event of timelineEvents) {
    if (!eventNames.has(event.name)) {
        continue;
    }
    if (event.args.data.requestId === htmlRequest.params.requestId) {
      matchingEventsForRequest.push(event)
    }
  }

  testRunner.log(`\nTrace events for index.html request`);
  for (const event of matchingEventsForRequest) {
    testRunner.log(`\n======= ${event.name} ======`)
      tracingHelper.logEventShape(event, ['headers'], ['name', 'resourceType', 'isLinkPreload', 'fetchPriorityHint', 'fetchType', 'protocol']);
    if (event.args.data.headers && event.args.data.headers.length) {
      // We found the exact list of headers was flakey, so we just instead check that we got some headers.
      testRunner.log(`${event.name} has args.data.headers`)
    }
  }

  testRunner.completeTest();
});
