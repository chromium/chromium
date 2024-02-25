(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const {session, dp} = await testRunner.startBlank('Tests requests queueing time consistency beetween the Network and Tracing domains.');
  const resourceSendRequest = 'ResourceSendRequest';
  const resourceWillSendRequest = 'ResourceWillSendRequest';
  // Request count = basic.html + 3 resources.
  const requestCount = 4;

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Network.enable();
  await tracingHelper.startTracing();

  const requestsFromNetworkDomain = new Map();

  dp.Network.onRequestWillBeSent(event => {
    requestsFromNetworkDomain.set(
      event.params.requestId,
      {
        url: event.params.request.url,
        // Timestamp from the network domain arrives in seconds.
        // We convert it to microseconds to compare it against
        // data coming from the tracing domain.
        timestamp: event.params.timestamp * 1000 * 1000
      });
  });

  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'});

  // Wait for traces to show up.
  for (let i = 0; i < requestCount; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  const devtoolsEvents = await tracingHelper.stopTracing();

  const requestsFromTracingDomain = new Map();
  for (const event of devtoolsEvents) {
    if (event.name !==  resourceSendRequest && event.name !== resourceWillSendRequest) {
      continue;
    }
    const requestId = event.args.data.requestId;
    const cachedEventInfo = requestsFromTracingDomain.get(requestId);

    // If a request is initiated by the browser process the start time is marked by a
    // "ResourceWillSendRequest" record. If, instead, the request is initiated by the
    // renderer, the time is marked by "ResourceSendRequest" one. In the first case,
    // it is possible that a "ResourceSendRequest" is issued as  well for the request
    // and thus it must be discarded in favor of the time marked by the
    // "ResourceWillSendRequest" record. In the latter case, no
    // "ResourceWillSendRequest" record is issued for the request.
    const mustOverwrite = cachedEventInfo && cachedEventInfo.name === resourceSendRequest && event.name === resourceWillSendRequest;
    if (!cachedEventInfo || mustOverwrite) {
      // Timestamp from tracing domain arrives in microseconds.
      requestsFromTracingDomain.set(requestId, {name: event.name, timestamp: event.ts, url: event.args.data.url} );
    }
  }

  const sortedEvents = [...requestsFromTracingDomain.entries()].sort((entry1, entry2) => {
    const url1 = entry1[1].url;
    const url2 = entry2[1].url;
    return url1.localeCompare(url2);
  });

  for (const [requestId, request] of sortedEvents) {
    const networkEvent = requestsFromNetworkDomain.get(requestId);
    // Compare at tenths-of-milliseconds resolution.
    const diff = Math.floor(Math.abs(networkEvent.timestamp - request.timestamp)  / 100);
    testRunner.log(`Queueing time difference of ${diff} for URL ${new URL(networkEvent.url).pathname}`);
  }

  testRunner.completeTest();
})
