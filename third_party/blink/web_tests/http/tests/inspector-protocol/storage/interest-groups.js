(async function(testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests that interest groups are read and cleared.`);
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  // Order by phase.
  function typeSortKey(type) {
    switch (type) {
      case 'join':
        return 0;
      case 'loaded':
        return 1;
      case 'bid':
        return 2;
      case 'win':
        return 3;
      default:
        return type;
    }
  }

  // Helper for sorting IG devtools events.
  function compareEvents(a, b) {
    let aTypeOrder = typeSortKey(a.type);
    let bTypeOrder = typeSortKey(b.type);
    return (aTypeOrder - bTypeOrder) ||
        a.ownerOrigin.localeCompare(b.ownerOrigin, 'en') ||
        a.name.localeCompare(b.name, 'en');
  }

  async function joinInterestGroups(id) {
    const joinJs = `
    navigator.joinAdInterestGroup({
        name: ${id},
        owner: "${baseOrigin}",
        biddingLogicURL: "${base}fledge_bidding_logic.js.php",
        ads: [{
          renderURL: 'https://example.com/render' + ${id},
          metadata: {ad: 'metadata', here: [1, 2, 3]}
        }]
      }, 3000)`;
    return session.evaluateAsync(joinJs);
  }

  async function runAdAuctionAndNavigateFencedFrame() {
    const auctionJs = `
      (async function() {
        config = await navigator.runAdAuction({
            decisionLogicURL: "${base}fledge_decision_logic.js.php",
            seller: "${baseOrigin}",
            interestGroupBuyers: ["${baseOrigin}"],
            resolveToConfig: true});

        const fencedFrame = document.createElement("fencedframe");
        fencedFrame.config = config;
        document.body.appendChild(fencedFrame);
      })();`;
    return session.evaluateAsync(auctionJs);
  }

  events = [];
  async function logAndClearEvents() {
    testRunner.log(`Events logged: ${events.length}`);
    // We need to sort events before dumping since ordering of bids is not
    // deterministic.
    events.sort(compareEvents);
    for (event of events) {
      testRunner.log(
        JSON.stringify(event, ['ownerOrigin', 'name', 'type'], 2));
      data = await dp.Storage.getInterestGroupDetails(
        {ownerOrigin: event.ownerOrigin, name: event.name});
      const details = data.result.details;
      details.expirationTime = 0;
      testRunner.log(details);
    }
    events = [];
  }

  let resolveWaitForWinPromise;
  const waitForWinPromise = new Promise((resolve, reject)=>{
    resolveWaitForWinPromise = resolve;
  });

  dp.Storage.onInterestGroupAccessed((messageObject)=>{
    events.push(messageObject.params);
    if (messageObject.params.type == 'win') {
      resolveWaitForWinPromise();
    }
  });
  await page.navigate(base + 'empty.html');

  // Start tracking, join interest groups, and run an auction.
  await dp.Storage.setInterestGroupTracking({enable: true});
  testRunner.log("Start Tracking");
  await joinInterestGroups(0);
  await joinInterestGroups(1);
  // Need to navigate a fenced frame to the winning ad for the bids to be
  // recorded.
  await runAdAuctionAndNavigateFencedFrame();
  // Have to wait for the win to be received, which happens after commit
  // (which also can't be waited for). Only do this if FLEDGE is enabled
  // and has sent events already, to avoid waiting for events that will
  // never occur.
  if (events.length > 0)
    await waitForWinPromise;
  await logAndClearEvents();

  // Disable interest group logging, and run the same set of events. No new
  // events should be logged. This has to be done after the logging test
  // because there's no way to wait until something doesn't happen, and
  // the logging of the bid events is potentially racy with enabling/disabling
  // logging.
  await dp.Storage.setInterestGroupTracking({enable: false});
  testRunner.log("Stop Tracking");
  // These calls should not trigger any events, since tracking is disabled.
  await joinInterestGroups(0);
  await joinInterestGroups(1);
  await runAdAuctionAndNavigateFencedFrame();
  logAndClearEvents();

  testRunner.completeTest();
  })
