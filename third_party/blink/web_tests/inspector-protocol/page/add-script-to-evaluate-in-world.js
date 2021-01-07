(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnNewDocument is executed in the given world');
  dp.Runtime.enable();
  dp.Page.enable();

  const scriptIds = [];
  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));
  dp.Runtime.onExecutionContextCreated(msg => {
    if (msg.params.context.name.includes('world'))
      testRunner.log(msg.params.context.name);
  });

  testRunner.log('Adding scripts');
  for (let i = 0; i < 5; ++i) {
    const result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
      console.log('message from ${i}');`, worldName: `world#${i}`});
    scriptIds.push(result.result.identifier);
  }

  await session.navigate('../resources/blank.html');

  testRunner.log('Removing scripts');
  for (let identifier of scriptIds) {
    const response = await dp.Page.removeScriptToEvaluateOnNewDocument({identifier});
    if (!response.result)
      testRunner.log('Failed script removal');
  }

  await session.navigate('../resources/blank.html');

  testRunner.completeTest();
})
