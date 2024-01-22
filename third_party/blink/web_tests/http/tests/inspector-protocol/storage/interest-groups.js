(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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

  let nextAuctionId = 1;
  let auctionIdMap = new Map();
  function normalizeAuctionId(event) {
    if ('uniqueAuctionId' in event) {
      if (!auctionIdMap.has(event.uniqueAuctionId)) {
        auctionIdMap.set(event.uniqueAuctionId, nextAuctionId);
        ++nextAuctionId;
      }
      return auctionIdMap.get(event.uniqueAuctionId);
    } else {
      return 'global';
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
  auctionEvents = [];
  async function logAndClearEvents() {
    testRunner.log('Logged IG events:');
    // We expect only one auction event, so no ordering issue to worry about.
    for (let event of auctionEvents) {
      event.uniqueAuctionId = normalizeAuctionId(event);

      // Only some of auctionConfig fields are kept so this doesn't have to be
      // changed every time something new is added that shows up by default.
      const keepConfigFields =
          new Set(['decisionLogicUrl', 'seller', 'interestGroupBuyers']);
      for (let fieldName of Object.getOwnPropertyNames(event.auctionConfig)) {
        if (!keepConfigFields.has(fieldName)) {
          delete event.auctionConfig[fieldName];
        }
      }
      testRunner.log(
          event, 'interestGroupAuctionEventOccurred ', ['eventTime']);
    }

    // We need to sort IG events before dumping since ordering of bids is not
    // deterministic.
    events.sort(compareEvents);
    for (let event of events) {
      event.uniqueAuctionId = normalizeAuctionId(event);
      testRunner.log(event, 'interestGroupAccessed ', ['accessTime']);
      data = await dp.Storage.getInterestGroupDetails(
        {ownerOrigin: event.ownerOrigin, name: event.name});
      const details = data.result.details;
      details.expirationTime = 0;
      testRunner.log(details, 'interestGroupDetails ');
    }
    auctionEvents = [];
    events = [];
  }

  let resolveWaitForWinPromise;
  const waitForWinPromise = new Promise((resolve, reject) => {
    resolveWaitForWinPromise = resolve;
  });

  dp.Storage.onInterestGroupAuctionEventOccurred(messageObject => {
    auctionEvents.push(messageObject.params);
  });

  dp.Storage.onInterestGroupAccessed(messageObject => {
    events.push(messageObject.params);
    if (messageObject.params.type == 'win') {
      resolveWaitForWinPromise();
    }
  });
  await page.navigate(base + 'empty.html');

  // Start tracking, join interest groups, and run an auction.
  await dp.Storage.setInterestGroupTracking({enable: true});
  await dp.Storage.setInterestGroupAuctionTracking({enable: true});
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
  testRunner.log('Stop Tracking IG Events');
  // These calls should only trigger auction events, since IG tracking is
  // disabled.
  await joinInterestGroups(0);
  await joinInterestGroups(1);
  await runAdAuctionAndNavigateFencedFrame();
  logAndClearEvents();

  testRunner.log('Stop Tracking Auction Events');
  await dp.Storage.setInterestGroupAuctionTracking({enable: false});
  // Now nothing should show up.
  await joinInterestGroups(0);
  await joinInterestGroups(1);
  await runAdAuctionAndNavigateFencedFrame();
  logAndClearEvents();
  testRunner.log('Test Done')

  testRunner.completeTest();
})
