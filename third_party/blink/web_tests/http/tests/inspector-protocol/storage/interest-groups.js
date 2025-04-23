const publicKeyConfig = `{
  "originScopedKeys": {
    "https://a.test:8443": {
      "keys":[{
        "id":"14345678-9abc-def0-1234-56789abcdef0",
        "key":"oV9AZYb6xHuZWXDxhdnYkcdNzx65Gn1QpYsBaD5gBS0="}]
    }
  }
}`;

// 32 random'ish bits in hex.
function rand32() {
  return Math.abs((Math.random() * 0x100000000) & 0xFFFFFFFF).toString(16)
}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests that interest groups are read and cleared.`);
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  // We generate a random coordinator hostname to isolate tests.
  let coordinator = "https://cd" + rand32() + rand32() + rand32() + rand32() +
                    ".test";
  await dp.Browser.addPrivacySandboxCoordinatorKeyConfig({
    api: 'TrustedKeyValue',
    coordinatorOrigin: coordinator,
    keyConfig: publicKeyConfig
  });

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
  function normalizeAuctionId(uniqueAuctionId) {
    if (uniqueAuctionId) {
      if (!auctionIdMap.has(uniqueAuctionId)) {
        auctionIdMap.set(uniqueAuctionId, nextAuctionId);
        ++nextAuctionId;
      }
      return auctionIdMap.get(uniqueAuctionId);
    } else {
      return 'global';
    }
  }

  function keepOnlyFields(object, keepFields) {
    for (let fieldName of Object.getOwnPropertyNames(object)) {
      if (!keepFields.has(fieldName)) {
        delete object[fieldName];
      }
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

  // Helper for sorting auction <-> network events. Only cares about types
  // since it's good enough for this application, and everything else is
  // a random ID.
  function compareNetEvents(a, b) {
    return a.type.localeCompare(b.type, 'en');
  }

  async function joinInterestGroups(dp, id) {
    const joinJs = `
    navigator.joinAdInterestGroup({
        name: ${id},
        owner: "${baseOrigin}",
        biddingLogicURL: "${base}fledge_bidding_logic.js.php",
        trustedBiddingSignalsURL: "${base}fledge_bidding_signals.js.php",
        trustedBiddingSignalsCoordinator: "${coordinator}",
        ads: [{
          renderURL: 'https://example.com/render' + ${id},
          metadata: {ad: 'metadata', here: [1, 2, 3]}
        }]
      }, 3000)`;
    let result = await session.evaluateAsync(joinJs);
    function b64(array) {
      return btoa(String.fromCharCode.apply(null, array));
    }
    let hashes = [];
    const encoder = new TextEncoder();
    const kBidAnonKey = encoder.encode(
        `AdBid\n${baseOrigin}\n${base}fledge_bidding_logic.js.php\nhttps://example.com/render${id}`);
    hashes.push(b64(new Uint8Array(await window.crypto.subtle.digest('SHA-256', kBidAnonKey))));
    const kReportAnonKey = encoder.encode(
        `NameReport\n${baseOrigin}\n${base}fledge_bidding_logic.js.php\nhttps://example.com/render${id}\n${id}`);
    hashes.push(b64(new Uint8Array(await window.crypto.subtle.digest('SHA-256', kReportAnonKey))));
    await dp.Storage.setProtectedAudienceKAnonymity(
      {owner: baseOrigin, name: String(id).toWellFormed(), hashes: hashes});
    return result;
  }

  async function runAdAuctionAndNavigateFencedFrame() {
    const auctionJs = `
      (async function() {
        config = await navigator.runAdAuction({
            decisionLogicURL: "${base}fledge_decision_logic.js.php",
            trustedScoringSignalsURL: "${base}fledge_scoring_signals.js.php",
            trustedScoringSignalsCoordinator: "${coordinator}",
            seller: "${baseOrigin}",
            interestGroupBuyers: ["${baseOrigin}"],
            resolveToConfig: true});

        const fencedFrame = document.createElement("fencedframe");
        fencedFrame.config = config;
        document.body.appendChild(fencedFrame);
      })();`;
    return session.evaluateAsync(auctionJs);
  }

  let networkRequestUrls = new Map();

  let events = [];
  let auctionEvents = [];
  let auctionNetworkEvents = [];
  async function logAndClearEvents() {
    testRunner.log('Logged IG events:');
    // We expect only one auction event, so no ordering issue to worry about.
    for (let event of auctionEvents) {
      event.uniqueAuctionId = normalizeAuctionId(event.uniqueAuctionId);

      // Only some of auctionConfig fields are kept so this doesn't have to be
      // changed every time something new is added that shows up by default.
      const keepConfigFields =
          new Set(['decisionLogicURL', 'seller', 'interestGroupBuyers']);
      keepOnlyFields(event.auctionConfig, keepConfigFields);
      testRunner.log(
          event, 'interestGroupAuctionEventOccurred ', ['eventTime']);
    }

    auctionNetworkEvents.sort(compareNetEvents);
    for (let event of auctionNetworkEvents) {
      event.auctions = event.auctions.map((a) => normalizeAuctionId(a));
      event.url = networkRequestUrls.get(event.requestId);
      testRunner.log(event, 'interestGroupAuctionNetworkRequestCreated ');
    }

    // We need to sort IG events before dumping since ordering of bids is not
    // deterministic.
    events.sort(compareEvents);
    for (let event of events) {
      event.uniqueAuctionId = normalizeAuctionId(event.uniqueAuctionId);
      testRunner.log(event, 'interestGroupAccessed ', ['accessTime']);
      data = await dp.Storage.getInterestGroupDetails(
        {ownerOrigin: event.ownerOrigin, name: event.name});
      const details = data.result.details;
      const keepIgFields =
          new Set(['adComponents', 'ads', 'biddingLogicURL',
                   'executionMode', 'joiningOrigin', 'name',
                   'ownerOrigin', 'priority', 'trustedBiddingSignalsKeys',
                   'trustedBiddingSignalsURL']);
      keepOnlyFields(details, keepIgFields);
      testRunner.log(details, 'interestGroupDetails ');
    }
    events = [];
    auctionEvents = [];
    auctionNetworkEvents = [];
    initWaitForReportingComplete();
  }

  let waitForReportingPromise, resolveWaitForReportingCompletePromise;
  function initWaitForReportingComplete() {
    waitForReportingPromise = new Promise((resolve, reject) => {
      resolveWaitForReportingCompletePromise = resolve;
    });
  }
  initWaitForReportingComplete();

  dp.Storage.onInterestGroupAuctionEventOccurred(messageObject => {
    auctionEvents.push(messageObject.params);
  });

  dp.Storage.onInterestGroupAuctionNetworkRequestCreated(messageObject => {
    auctionNetworkEvents.push(messageObject.params);
  });

  dp.Storage.onInterestGroupAccessed(messageObject => {
    events.push(messageObject.params);
  });

  dp.Network.onRequestWillBeSent(messageObject => {
    networkRequestUrls.set(
        messageObject.params.requestId, messageObject.params.request.url);
    if (messageObject.params.request.url ===
        'https://a.test:8443/echoall?report_bidder') {
      resolveWaitForReportingCompletePromise();
    }
  });

  await page.navigate(base + 'empty.html');

  // Enable network events, to check cross-referencing of them.
  await dp.Network.enable();

  // Start tracking, join interest groups, and run an auction.
  await dp.Storage.setInterestGroupTracking({enable: true});
  await dp.Storage.setInterestGroupAuctionTracking({enable: true});
  testRunner.log("Start Tracking");
  await joinInterestGroups(dp, 0);
  await joinInterestGroups(dp, 1);
  // Need to navigate a fenced frame to the winning ad for the bids to be
  // recorded.
  await runAdAuctionAndNavigateFencedFrame();
  // Wait for reporting to finish. This will:
  // 1) Guarantee that all the events we expect to have happened, since they
  //    happen-before reporting.
  // 2) Make sure that the bidding worklet is released before the second run
  //    and does not get shared.
  await waitForReportingPromise;
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
  await joinInterestGroups(dp, 0);
  await joinInterestGroups(dp, 1);
  await runAdAuctionAndNavigateFencedFrame();
  await waitForReportingPromise;
  logAndClearEvents();

  testRunner.log('Stop Tracking Auction Events');
  await dp.Storage.setInterestGroupAuctionTracking({enable: false});
  // Now nothing should show up.
  await joinInterestGroups(dp, 0);
  await joinInterestGroups(dp, 1);
  await runAdAuctionAndNavigateFencedFrame();
  logAndClearEvents();
  testRunner.log('Test Done')

  testRunner.completeTest();
})
