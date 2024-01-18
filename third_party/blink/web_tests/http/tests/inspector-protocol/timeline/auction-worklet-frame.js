(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const bp = testRunner.browserP();
  const {page, session, dp} = await testRunner.startBlank(
      'Tracing of FLEDGE worklets in subframes.',
      {url: base + 'fledge_join.html?40'});

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing('fledge');

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  const createChildFrame = function(url) {
    return new Promise(resolve => {
      const frame = document.createElement(`iframe`);
      frame.id = 'frame';
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  };

  // Navigate away to a cross-site context compared to the eventual frame,
  // now the join has completed.
  await page.navigate(
      'https://b.test:8443/inspector-protocol/resources/blank.html');

  // Enable auto-attach so we can talk to the frame's target easily.
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;

  const makeFrameJS = `
    (${createChildFrame.toString()})('${base + 'iframe.html'}')
  `;

  const frameTargetPromise = dp.Target.onceAttachedToTarget();
  await session.evaluateAsync(makeFrameJS);
  const frameTarget = await frameTargetPromise;
  const frameSession = session.createChild(frameTarget.params.sessionId);
  // Enable auto-attach for the frame, so we attach to auction worklet.
  frameSession.protocol.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const winner = await frameSession.evaluateAsync(auctionJs);
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
