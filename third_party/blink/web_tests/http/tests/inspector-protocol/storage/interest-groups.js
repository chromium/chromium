(async function(testRunner) {
  const {dp, page} = await testRunner.startBlank(
      `Tests that interest groups are read and cleared.`);
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  async function runAdAuction (page) {
    const auctionJs = `
    navigator.runAdAuction({
      decisionLogicUrl: "${base}fledge_decision_logic.js.php",
      seller: "${baseOrigin}",
      interestGroupBuyers: ["${baseOrigin}"]})`;

    const pageSession = await page.createSession();
    return pageSession.evaluateAsync(auctionJs);
  }

  await dp.Page.enable();

  const events = [];
  dp.Storage.onInterestGroupAccessed((messageObject)=>{events.push(messageObject.params)});
  await dp.Storage.setInterestGroupTracking({enable: false});

  // These navigations should not trigger any events, since tracking is
  // disabled.
  await dp.Page.navigate({url: base + 'fledge_join.html'});

  await runAdAuction(page);

  testRunner.log(`Events logged: ${events.length}`);

  await dp.Storage.setInterestGroupTracking({enable: true});
  testRunner.log("Start Tracking");

  await dp.Page.navigate({url: base + 'fledge_join.html'});

  await runAdAuction(page);

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