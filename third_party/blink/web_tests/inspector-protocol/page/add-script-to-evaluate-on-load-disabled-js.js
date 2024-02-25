(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnLoad is executed with disabled javascript');
  await dp.Page.enable();
  await dp.Emulation.setScriptExecutionDisabled({value: true});
  await dp.Page.addScriptToEvaluateOnNewDocument({source: `window.FOO = 42;`});

  await session.navigate('../resources/blank.html');
  testRunner.log(await session.evaluate('window.FOO'));
  testRunner.completeTest();
})
