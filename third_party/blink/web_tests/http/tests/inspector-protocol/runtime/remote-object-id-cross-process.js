(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that RemoteObjectId is unique across processes.`);

  testRunner.log('Navigating twice to ensure fresh process');
  await page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/empty.html');
  await page.navigate('http://localhost:8000/inspector-protocol/resources/empty.html');
  const evaluateResponse1 = await dp.Runtime.evaluate({ expression: '({ foo: 42 })' });
  testRunner.log(evaluateResponse1, 'Runtime.evaluate ');

  testRunner.log('Navigating cross-process');
  await page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/empty.html');
  const evaluateResponse2 = await dp.Runtime.evaluate({ expression: '({ foo: 17 })' });
  testRunner.log(evaluateResponse2, 'Runtime.evaluate ');

  const callFunctionOnResponse = await dp.Runtime.callFunctionOn({
      functionDeclaration: 'function() { return this.foo }',
      objectId: evaluateResponse1.result.result.objectId,
  });
  // This should be an error, because the first process is long gone.
  testRunner.log(callFunctionOnResponse, 'Runtime.callFunctionOn ');

  testRunner.completeTest();
})
