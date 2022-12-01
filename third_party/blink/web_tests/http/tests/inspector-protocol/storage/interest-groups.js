(async function(testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests that interest groups are read and cleared.`);
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  async function joinInterestGroups(id) {
    const joinJs = `
    navigator.joinAdInterestGroup({
        name: ${id},
        owner: "${baseOrigin}",
        biddingLogicUrl: "${base}fledge_bidding_logic.js.php",
        ads: [{
          renderUrl: 'https://example.com/render' + ${id},
          metadata: {ad: 'metadata', here: [1, 2, 3]}
        }]
      }, 3000)`;
    return session.evaluateAsync(joinJs);
  }

  async function runAdAuction() {
    const auctionJs = `
    navigator.runAdAuction({
      decisionLogicUrl: "${base}fledge_decision_logic.js.php",
      seller: "${baseOrigin}",
      interestGroupBuyers: ["${baseOrigin}"]})`;
    return session.evaluateAsync(auctionJs);
  }

  const events = [];
  dp.Storage.onInterestGroupAccessed((messageObject)=>{events.push(messageObject.params)});
  await dp.Storage.setInterestGroupTracking({enable: false});

  await page.navigate(base + 'empty.html');

  // These calls should not trigger any events, since tracking is disabled.
  await joinInterestGroups(0);
  await joinInterestGroups(1);
  await runAdAuction();

  testRunner.log(`Events logged: ${events.length}`);

  await dp.Storage.setInterestGroupTracking({enable: true});
  testRunner.log("Start Tracking");

  await joinInterestGroups(0);
  await joinInterestGroups(1);
  await runAdAuction();

  for (event of events) {
    testRunner.log(
      JSON.stringify(event, ['ownerOrigin', 'name', 'type'], 2));
    data = await dp.Storage.getInterestGroupDetails(
      {ownerOrigin: event.ownerOrigin, name: event.name});
    const details = data.result.details;
    details.expirationTime = 0;
    testRunner.log(details);
  }
  testRunner.completeTest();
  })