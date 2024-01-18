(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    'Tests Page.domContentEventFired is triggered before Page.loadEventFired for a non-blank page');

  await dp.Page.enable();

  const htmlPage = testRunner.url('../resources/dom-snapshot.html');
  dp.Page.navigate({ url: htmlPage });
  const domContentEvent = await dp.Page.onceDomContentEventFired();
  testRunner.log(domContentEvent);
  const loadEvent = await dp.Page.onceLoadEventFired();
  testRunner.log(loadEvent);

  testRunner.completeTest();
})
