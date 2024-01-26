(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnLoad has access to command line APIs');
  dp.Runtime.enable();
  dp.Page.enable();

  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));

  testRunner.log('Without an isolated world:');
  const scriptIdentifiers = [];

  let result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('1 ' + monitorEvents.toString());
  `, includeCommandLineAPI: true});

  scriptIdentifiers.push(result.result.identifier);

  result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('2 ' + monitorEvents.toString());
  `, includeCommandLineAPI: true});

  scriptIdentifiers.push(result.result.identifier);

  result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('3 ' + typeof window.monitorEvents);
  `, includeCommandLineAPI: false});

  scriptIdentifiers.push(result.result.identifier);

  await session.navigate('../resources/blank.html');

  for (const identifier of scriptIdentifiers) {
    await dp.Page.removeScriptToEvaluateOnNewDocument({identifier});
  }

  testRunner.log('With an isolated world:');
  await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('1 ' + monitorEvents.toString());
  `, includeCommandLineAPI: true, worldName: 'inspector-tests'});
  await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('2 ' + monitorEvents.toString());
  `, includeCommandLineAPI: true, worldName: 'inspector-tests'});
  await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.log('3 ' + typeof window.monitorEvents);
  `, includeCommandLineAPI: false, worldName: 'inspector-tests'});

  await session.navigate('../resources/blank.html');

  testRunner.completeTest();
})
