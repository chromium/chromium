(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnLoad is executed in the order of addition');
  dp.Runtime.enable();
  dp.Page.enable();

  const scriptIds = [];
  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));

  testRunner.log('Adding scripts');
  for (let i = 0; i < 5; ++i) {
    const result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
      console.log('message from ${i}');
    `});
    scriptIds.push(result.result.identifier);
  }

  testRunner.log('Navigating same-process');
  await session.navigate('../resources/blank.html');

  testRunner.log('Navigating cross-process');
  await session.navigate('http://127.0.0.1:8000/inspector-protocol/resources/empty.html');

  testRunner.log('Removing scripts');
  for (let identifier of scriptIds) {
    const response = await dp.Page.removeScriptToEvaluateOnNewDocument({identifier});
    if (!response.result)
      testRunner.log('Failed script removal');
  }

  testRunner.log('Navigating cross-process again');
  await session.navigate('../resources/blank.html');

  // Dummy evaluate to wait for all scripts if any.
  await session.evaluate(`1`);

  testRunner.completeTest();
})
