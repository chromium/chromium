(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const base = 'https://a.test:8443/inspector-protocol/resources/'
  const bp = testRunner.browserP();
  const {page, session, dp} = await testRunner.startBlank(
      'Basic functionality of debugging FLEDGE worklets.',
      {url: base + 'fledge_join.html'});

  function handleUrn(input) {
    if (typeof input === 'string' && input.startsWith('urn:uuid:'))
      return 'urn:uuid:(randomized)';
    return input;
  }

  let attachedWorklets = 0;
  let resolveWaitForThirdWorkletPromise;
  const waitForThirdWorkletPromise = new Promise((resolve, reject)=>{
    resolveWaitForThirdWorkletPromise = resolve;
  });

  bp.Target.onAttachedToTarget(async event => {
    const params = event.params;
    testRunner.log(`Attached to target ${params.targetInfo.url}`);
    if (params.waitingForDebugger) {
      const targetSession =
          new TestRunner.Session(testRunner, event.params.sessionId);
      if (params.targetInfo.type === 'auction_worklet') {
        ++attachedWorklets;
        if (attachedWorklets == 4)
          resolveWaitForThirdWorkletPromise();
        testRunner.log('  Bidder instrumentation breakpoint set attempted');
        targetSession.protocol.Debugger.enable();
        targetSession.protocol.Runtime.enable();
        targetSession.protocol.Debugger.onPaused(async event => {
          let topFrame = event.params.callFrames[0];
          let reason = event.params.reason;
          testRunner.log(`Paused at ${topFrame.url} with reason "${reason}".`);
          targetSession.protocol.Debugger.resume();
        });
        targetSession.protocol.Runtime.onConsoleAPICalled(event => {
          const params = event.params;
          const topFrame = params.stackTrace.callFrames[0];
          testRunner.log(`Console call of type: ${params.type} ` +
                         `arg0: ${params.args[0].value} ` +
                         `at ${topFrame.url}:${topFrame.lineNumber}`);
        });

        await targetSession.protocol.EventBreakpoints.setInstrumentationBreakpoint(
            {eventName: 'beforeBidderWorkletBiddingStart'});
      }
      targetSession.protocol.Runtime.runIfWaitingForDebugger();
    }
  });

  const auctionJs = `
    navigator.runAdAuction({
      decisionLogicURL: "${base}fledge_decision_logic.js.php",
      seller: "https://a.test:8443",
      interestGroupBuyers: ["https://a.test:8443"]})`;

  // Ask to auto-attach related before running the auction.
  await bp.Target.autoAttachRelated(
      {targetId: page.targetId(), waitForDebuggerOnStart: true});

  const winner = await session.evaluateAsync(auctionJs);

  // Have to wait for the third worklet to be attached (first the seller
  // is attached, then the bidder is attached to bid, and then the
  // bidder worklet is reloaded to call reportWin()), as reportWin()
  // is invoked racily with reporting auction completion. Only do this
  // if FLEDGE is enabled and has sent events already, to avoid waiting
  // for events that will never occur.
  if (attachedWorklets > 0)
    await waitForThirdWorkletPromise;

  testRunner.log('Auction winner:' + handleUrn(winner));
  testRunner.log('DONE');
  testRunner.completeTest();
});
