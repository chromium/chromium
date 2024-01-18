(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const bp = testRunner.browserP();
  const {page, session, dp} = await testRunner.startBlank(
      'Tracing of FLEDGE worklets.', {url: base + 'fledge_join.html?40'});

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing(
      'fledge,disabled-by-default-devtools.timeline');

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  function verifyAuctionProcessEventData(data) {
    if (data.type !== 'bidder' && data.type !== 'seller') {
      testRunner.log(
          'Weird AuctionWorkletRunningInProcess event type:' + data.type);
    }
    if (isNaN(parseInt(data.pid))) {
      testRunner.log(
          'Weird AuctionWorkletRunningInProcess event pid:' + data.pid);
    }
    if (!('target' in data)) {
      testRunner.log('AuctionWorkletRunningInProcess event missing target');
    }
  }

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;


  const winner = await session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner));

  const devtoolsEvents =
      await tracingHelper.stopTracing(/fledge|devtools.timeline/);

  let sawBidders = 0;
  let sawSellers = 0;
  // For process ones, we don't count them since the particulars depend on
  // whether this is on Android or not.
  let sawBidderRunningInProcess = 'nope';
  let sawSellerRunningInProcess = 'nope';
  let sawBidderDoneWithProcess = 'nope';
  for (ev of devtoolsEvents) {
    if (ev.name === 'AuctionWorkletRunningInProcess') {
      let data = ev.args.data;
      if (data.type === 'bidder') {
        sawBidderRunningInProcess = data.host;
      } else if (data.type === 'seller') {
        sawSellerRunningInProcess = data.host;
      }
      verifyAuctionProcessEventData(data);
    }
    if (ev.name === 'AuctionWorkletDoneWithProcess') {
      let data = ev.args.data;
      if (data.type === 'bidder') {
        sawBidderDoneWithProcess = data.host;
      }
      // Note that seller unload is not guaranteed to be observed, as it can
      // happen after auction completion.
      verifyAuctionProcessEventData(data);
    }
    if (ev.name === 'generate_bid')
      ++sawBidders;
    if (ev.name === 'score_ad')
      ++sawSellers;
  }

  if (sawBidders >= 30) {
    testRunner.log('Saw enough bidders');
  } else {
    testRunner.log('Saw too few bidders:' + sawBidders);
  }

  if (sawSellers >= 30) {
    testRunner.log('Saw enough sellers');
  } else {
    testRunner.log('Saw too few sellers:' + sawSellers);
  }
  testRunner.log(
      'Saw process assignment for bidder for host:' +
      sawBidderRunningInProcess);
  testRunner.log(
      'Saw process assignment for seller for host:' +
      sawSellerRunningInProcess);
  testRunner.log(
      'Saw process release for bidder for host:' + sawBidderDoneWithProcess);
  // Note that seller unload is not guaranteed to be observed, as it can happen
  // after auction completion.

  testRunner.completeTest();
})
