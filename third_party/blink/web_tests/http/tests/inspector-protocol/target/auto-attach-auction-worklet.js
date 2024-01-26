(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests auto-attach to FLEDGE worklets.');
  const bp = testRunner.browserP();

  const urlForSession = new Map();

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  function normalizeTitle(input) {
    // There is some flakiness with titles of about:blank frame targets
    // sometimes being '' and sometimes 'about:blank'.
    if (input === 'about:blank')
      return '';
    return input;
  }

  let detachedBidderWorklets = 0;
  let resolveWaitForSecondBidderWorkletDetachedPromise;
  const waitForSecondBidderWorkletDetachedPromise = new Promise((resolve, reject)=>{
    resolveWaitForSecondBidderWorkletDetachedPromise = resolve;
  });
  bp.Target.onAttachedToTarget(event => {
    const params = event.params;
    testRunner.log(
        `Attached to ${params.targetInfo.type} target: ${params.targetInfo.url}
    (title: ${normalizeTitle(params.targetInfo.title)})
    (waiting: ${params.waitingForDebugger})`);
    urlForSession.set(params.sessionId, params.targetInfo.url);
  });

  bp.Target.onDetachedFromTarget(event => {
    let url = urlForSession.get(event.params.sessionId);
    testRunner.log('Detached: ' + url);
    if (url.endsWith("fledge_bidding_logic.js.php")) {
      ++detachedBidderWorklets;
      if (detachedBidderWorklets == 2)
        resolveWaitForSecondBidderWorkletDetachedPromise();
    }
  });

  const base = 'https://a.test:8443/inspector-protocol/resources/'

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;

  const page1 = await testRunner.createPage({url: base + 'fledge_join.html'});
  const page1_session = await page1.createSession();

  // Run auction w/o auto-attach.
  const winner1 = await page1_session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner1));

  // Ask to auto-attach related before re-running the auction. Use another page to
  // avoid reporting events that happen after the auction completes.
  const page2 = await testRunner.createPage({url: base + 'fledge_join.html'});
  const page2_session = await page2.createSession();
  await bp.Target.autoAttachRelated(
      {targetId: page2.targetId(), waitForDebuggerOnStart: false});

  const winner2 = await page2_session.evaluateAsync(auctionJs);
  // If FLEDGE is enabled, need to wait for post-auction scripts to run, to
  // ensure a consistent number of FLEDGE scripts have been invoked.
  if (detachedBidderWorklets > 0)
    await waitForSecondBidderWorkletDetachedPromise;
  testRunner.log('Auction winner:' + handleUrn(winner2));
  testRunner.log('DONE');
  testRunner.completeTest();
});
