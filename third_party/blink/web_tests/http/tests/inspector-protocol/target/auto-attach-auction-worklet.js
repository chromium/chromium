(async function(testRunner) {
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

  bp.Target.onAttachedToTarget(event => {
    const params = event.params;
    testRunner.log(
        `Attached to ${params.targetInfo.type} target: ${params.targetInfo.url}
    (title: ${normalizeTitle(params.targetInfo.title)})
    (waiting: ${params.waitingForDebugger})`);
    urlForSession.set(params.sessionId, params.targetInfo.url);
  });

  bp.Target.onDetachedFromTarget(event => {
    testRunner.log('Dettached: ' + urlForSession.get(event.params.sessionId));
  });

  const base = 'https://a.test:8443/inspector-protocol/resources/'

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicUrl: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;

  const page1 = await testRunner.createPage({url: base + 'fledge_join.html'});
  const page1_session = await page1.createSession();

  // Run auction w/o auto-attach.
  const winner1 = await page1_session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner1));

  // Ask to auto-attach related before re-running the auction.
  await bp.Target.autoAttachRelated(
      {targetId: page1.targetId(), waitForDebuggerOnStart: false});

  const winner2 = await page1_session.evaluateAsync(auctionJs);
  testRunner.log('Auction winner:' + handleUrn(winner2));
  testRunner.log('DONE');
  testRunner.completeTest();
});
