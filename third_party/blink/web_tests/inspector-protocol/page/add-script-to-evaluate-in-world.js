(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnNewDocument is executed in the given world');

  dp.Page.enable();
  const mainFrameId = (await dp.Page.getFrameTree()).result.frameTree.frame.id;

  dp.Runtime.enable();
  const scriptIds = [];
  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));
  const logContextCreationCallback = msg => {
    const suffix = mainFrameId === msg.params.context.auxData.frameId ? 'main frame' : 'subframe';
    testRunner.log(`${msg.params.context.name || '<main world>'} in ${suffix}`);
  };
  dp.Runtime.onExecutionContextCreated(logContextCreationCallback);
  await dp.Runtime.onceExecutionContextCreated();

  testRunner.log('Adding scripts');
  for (let i = 0; i < 5; ++i) {
    const result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
      console.log('message from ${i}');`, worldName: `world#${i}`});
    scriptIds.push(result.result.identifier);
  }

  testRunner.log('Navigating');
  await session.navigate('../resources/blank.html');

  await session.evaluate(`
    {
      const iframe = document.createElement('iframe');
      // The url does not matter since we document.open immediately below.
      // It must not be about:blank though, to avoid sync commit.
      iframe.src = 'http://google.com';
      document.body.appendChild(iframe);
      console.log('added iframe');
      iframe.contentDocument.open();
      iframe.contentDocument.write('hello');
      iframe.contentDocument.close();
      console.log('written to iframe ' + iframe.contentDocument.documentElement.innerHTML);
    }
  `);

  await session.evaluate(`
    {
      const iframe = document.createElement('iframe');
      iframe.src = "javascript:'<html><body>Hey?</body></html>';";
      document.body.appendChild(iframe);
      console.log('added javascript iframe');
      iframe.contentDocument.open();
      iframe.contentDocument.write('world');
      iframe.contentDocument.close();
      console.log('written to iframe ' + iframe.contentDocument.documentElement.innerHTML);
    }
  `);

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
