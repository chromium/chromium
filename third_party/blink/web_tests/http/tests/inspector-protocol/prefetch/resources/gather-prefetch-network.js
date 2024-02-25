(async function gatherNetworkEvents(testRunner, dp, options = {}) {
  let { log, requests: requestsRemaining } = { log: true, requests: 1, ...options };

  const events = [];
  const extraInfoEvents = [];

  await dp.Network.enable();
  dp.Network.onRequestWillBeSent(event => events.push(event));
  dp.Network.onRequestWillBeSentExtraInfo(event => extraInfoEvents.push(event));
  dp.Network.onResponseReceived(event => events.push(event));
  dp.Network.onResponseReceivedExtraInfo(event => extraInfoEvents.push(event));

  await new Promise(resolve => {
    function requestComplete() {
      if (--requestsRemaining === 0) resolve();
    }
    dp.Network.onLoadingFinished(event => { events.push(event); requestComplete(); });
    dp.Network.onLoadingFailed(event => { events.push(event); requestComplete(); });
  });

  // Wait a little bit longer for the network service to catch up, if necessary.
  let expectedExtraInfoEvents = 0;
  for (let event of events) {
    if (event.method === 'Network.requestWillBeSent') {
      expectedExtraInfoEvents += (event.params.redirectResponse ? 2 : 1);
    } else if (event.method === 'Network.responseReceived') {
      expectedExtraInfoEvents++;
    }
  }
  while (extraInfoEvents.length < expectedExtraInfoEvents) {
    await dp.Network.onceResponseReceivedExtraInfo();
  }

  // Sort the events by request ID, preserving the order in which requests were
  // first seen in the main event stream, and the relative order of events for a
  // particular request ID.
  const requestOrder = Array.from(new Set(events.map(e => e.params.requestId)));
  const byRequestOrder = (a, b) => requestOrder.indexOf(a.params.requestId) - requestOrder.indexOf(b.params.requestId);
  events.sort(byRequestOrder);
  extraInfoEvents.sort(byRequestOrder);

  // The *ExtraInfo events are dispatched from the network service and may have
  // unreliable ordering relative to other events. To avoid flakiness, this test
  // weaves them after the corresponding (in sequence) non-ExtraInfo events.
  function takeWhile(arr, predicate) {
    const i = arr.findIndex(e => !predicate(e));
    return arr.splice(0, i >= 0 ? i : arr.length);
  }
  function maybeTake(arr, predicate) {
    if (arr.length > 0 && predicate(arr[0]))
      return arr.shift();
  }
  const unmatchedEvents = takeWhile(extraInfoEvents, e => !requestOrder.includes(e.params.requestId));
  for (let i = 0; i < events.length; i++) {
    unmatchedEvents.push(...takeWhile(extraInfoEvents, e => byRequestOrder(e, events[i]) < 0));
    switch (events[i].method) {
      case 'Network.requestWillBeSent':
        // As one particular quirk, responseReceived is not sent for redirect
        // responses; instead the response is attached to the requestWillBeSent.
        // As a result, we'll need to pick up both potential extra info events.
        const toPush = [
            events[i].params.redirectResponse && maybeTake(extraInfoEvents, e => e.method === 'Network.responseReceivedExtraInfo'),
            maybeTake(extraInfoEvents, e => e.method === 'Network.requestWillBeSentExtraInfo'),
        ];
        events.splice(i + 1, 0, ...toPush.filter(x => x));
        break;
      case 'Network.responseReceived':
        const responseExtra = maybeTake(extraInfoEvents, e => e.method === 'Network.responseReceivedExtraInfo');
        if (responseExtra)
          events.splice(i + 1, 0, responseExtra);
        break;
    }
  }

  if (log) {
    const stabilizeNames = [...TestRunner.stabilizeNames, 'connectionId', 'timing', 'connectTiming', 'wallTime', 'responseTime', 'securityDetails', 'remoteIPAddress', 'Date', 'ETag', 'Last-Modified', 'User-Agent', 'X-Powered-By', 'headersText'];
    for (const event of events) {
      testRunner.log(event.params, event.method, stabilizeNames);
    }

    if (unmatchedEvents.length) {
      testRunner.log(unmatchedEvents.length, 'unmatched events');
      for (const event of unmatchedEvents) {
        testRunner.log(event.params, event.method, stabilizeNames);
      }
    }
  }

  return events;
})
