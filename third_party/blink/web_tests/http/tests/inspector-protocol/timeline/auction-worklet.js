(async function(testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const bp = testRunner.browserP();
  const {page, session, dp} = await testRunner.startBlank(
      'Tracing of FLEDGE worklets.', {url: base + 'fledge_join.html?40'});

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('fledge');

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicUrl: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;


  const winner = await session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner));

  const devtoolsEvents = await tracingHelper.stopTracing(/fledge/);

  var sawBidders = 0;
  var sawSellers = 0;
  for (ev of devtoolsEvents) {
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

  testRunner.completeTest();
})
