(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    'Tests Page.domContentEventFired is triggered before Page.loadEventFired for a blank page'
  );

  await dp.Page.enable();

  const events = [];
  dp.Page.onDomContentEventFired((e) => events.push(e));
  dp.Page.onLoadEventFired((e) => events.push(e));

  const htmlPage = testRunner.url('../resources/blank.html');
  await dp.Page.navigate({ url: htmlPage });
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(events);
  testRunner.completeTest();
});
