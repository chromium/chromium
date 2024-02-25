(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const bp = testRunner.browserP();
  const {page, session, dp} = await testRunner.startBlank(
      'Tracing of network activity FLEDGE worklets.',
      {url: base + 'fledge_join.html?40'});
  const testStart = Date.now();
  const testLimit = 5 * 60 * 1000;  // Way longer than the test may take.

  dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('devtools.timeline');

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  function validateAbsoluteMs(object, field) {
    // The ms absolute format is from epoch, same as JS.
    if (field in object) {
      const val = object[field];
      if (val >= testStart && val < (testStart + testLimit))
        object[field] = '<absolute timestamp>';
    }
  }

  function validateRelativeMs(object, field) {
    if (field in object) {
      const val = object[field];
      if (val === -1 || (val >= 0.0 && val < testLimit))
        object[field] = '<relative timestamp>';
    }
  }

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;


  const winner = await session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner));

  const devtoolsEvents = await tracingHelper.stopTracing(/devtools.timeline/);

  const requestIdToInfo = new Map();
  for (ev of devtoolsEvents) {
    if (ev.name === 'ResourceSendRequest') {
      requestIdToInfo.set(ev.args.data.requestId, {
        url: ev.args.data.url,
        events: [{name: ev.name, data: ev.args.data}]
      });
    }
    if (ev.name === 'ResourceReceiveResponse' || ev.name === 'ResourceFinish') {
      requestIdToInfo.get(ev.args.data.requestId)
          .events.push({name: ev.name, data: ev.args.data});
    }
  }

  let sortedRequestInfo = [...requestIdToInfo.values()].sort((a, b) => {
    if (a.url < b.url)
      return -1;
    else if (a.url === b.url)
      return 0;
    return +1;
  });

  // There may racily be two bidding logic loads, depending on how far along running reportings scripts
  // has advanced, which is done after auction completion is signalled. If there are two such loads,
  // ignore the second one.
  if (sortedRequestInfo.length > 2 &&
      sortedRequestInfo[0].endsWith("fledge_bidding_logic.js.php") &&
      sortedRequestInfo[1].endsWith("fledge_bidding_logic.js.php")) {
    sortedRequestInfo = sortedRequestInfo.splice(1);
  }

  for (let requestInfo of sortedRequestInfo) {
    testRunner.log(requestInfo.url + ':');
    for (let ev of requestInfo.events) {
      const data = ev.data;
      validateAbsoluteMs(data, 'responseTime');

      if ('timing' in data) {
        validateRelativeMs(data.timing, 'connectEnd');
        validateRelativeMs(data.timing, 'connectStart');
        validateRelativeMs(data.timing, 'dnsEnd');
        validateRelativeMs(data.timing, 'dnsStart');
        validateRelativeMs(data.timing, 'receiveHeadersStart');
        validateRelativeMs(data.timing, 'receiveHeadersEnd');
        validateRelativeMs(data.timing, 'sendEnd');
        validateRelativeMs(data.timing, 'sendStart');
        validateRelativeMs(data.timing, 'sslEnd');
        validateRelativeMs(data.timing, 'sslStart');
      }

      // requestTime and finishTime are in TimeTicks, so their absolute values
      // can't be interpreted.
      testRunner.log(
          data, ev.name + ' ', ['requestId', 'requestTime', 'finishTime', 'value']);
    }
    testRunner.log('\n');
  }

  testRunner.completeTest();
})
