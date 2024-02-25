(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that targets created with window.open share the same browser context.`);

  await dp.Target.setDiscoverTargets({discover: true});
  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened a second window');
  await dp.Target.onceTargetCreated();
  const pageTargets = (await dp.Target.getTargets()).result.targetInfos.filter(target => target.type === 'page');
  const browserContextId = pageTargets[0].browserContextId;
  if (!browserContextId) {
    testRunner.log('Found a page target without a browserContextId!');
    testRunner.log(pageTargets);
    testRunner.completeTest();
    return;
  }
  for (const pageTarget of pageTargets) {
    if (pageTarget.browserContextId !== browserContextId) {
      testRunner.log('Targets have different browserContextIds!');
      testRunner.log(pageTargets);
      testRunner.completeTest();
      return;
    }
  }
  testRunner.log('SUCCESS: All ' + pageTargets.length + ' pages belong to the same browser context.');
  testRunner.completeTest();
})
