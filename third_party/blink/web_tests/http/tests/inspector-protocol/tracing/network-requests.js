(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Network request lifecycle trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Network.enable();

  await tracingHelper.startTracing('devtools.timeline');

  dp.Page.navigate(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

  // Get the ID of the request for the HTML page
  const htmlRequest = await dp.Network.onceRequestWillBeSent(e => {
    return e.params.request.url.includes('basic.html');
  });

  // Bind a listener for when the HTML request is completely finished.
  // Note that we don't await it yet as we want to wait for responses from
  // our 4 expected requests first.
  const htmlRequestDone = dp.Network.onceLoadingFinished(e => {
    return e.params.requestId === htmlRequest.params.requestId;
  });

  await new Promise(resolve => {
    let count = 0;
    dp.Network.onResponseReceived(() => {
        ++count;
      // We expect 4 requests:
      // - basic.html
      // - empty.js
      // - square.png
      // - style.css
        if (count === 4) resolve();
    });
  });

  // Ensure the HTML request is completely finished before any assertions
  await htmlRequestDone;

  const timelineEvents = await tracingHelper.stopTracing(/devtools.timeline/);

  const eventNames = new Set(['ResourceSendRequest', 'ResourceWillSendRequest', 'ResourceReceiveResponse', 'ResourceReceivedData', 'ResourceFinish']);
  const eventsByRequestId = new Map();
  const requestIdToUrl = new Map();

  for (const event of timelineEvents) {
    if (!eventNames.has(event.name)) {
        continue;
    }
    if (!event.args.data.requestId) continue;
    const events = eventsByRequestId.get(event.args.data.requestId) || [];
    events.push(event);
    eventsByRequestId.set(event.args.data.requestId, events);

    if (event.args.data.url) {
        requestIdToUrl.set(event.args.data.requestId, event.args.data.url);
    }
  }

  const orderedKeys = Array.from(requestIdToUrl.entries())
    .sort((a, b) => a[1].localeCompare(b[1]))
    .map(r => r[0]);
  for (const key of orderedKeys) {
    testRunner.log(`\nTrace events for URL: ${requestIdToUrl.get(key)}:`);
    const events = eventsByRequestId.get(key);
    for (const event of events) {
      tracingHelper.logEventShape(event, ['headers'], ['name', 'resourceType', 'isLinkPreload', 'fetchPriorityHint', 'fetchType', 'protocol']);
      if(event.name === 'ResourceReceiveResponse') {
        const headersCount = event.args.data.headers.length ?? 0;
        testRunner.log(`${event.name} has headers: ${headersCount > 0}\n`);
      }
    }
  }

  testRunner.completeTest();
});
