(async function(testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests that async stack tagging API is disabled by default.');

  dp.Runtime.enable();

  const response1 = await dp.Runtime.onceExecutionContextCreated();
  const pageContextId = response1.params.context.id;  // main page
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  const response2 = await dp.Runtime.onceExecutionContextCreated();
  const frameContextId = response2.params.context.id;  // IFrame

  const configs = [pageContextId, frameContextId];

  dp.Runtime.onConsoleAPICalled((result) => testRunner.log(result));
  dp.Runtime.onExceptionThrown((result) => testRunner.log(result));

  const code = `
    console.log(typeof console.scheduleAsyncTask);
    console.log(typeof console.startAsyncTask);
    console.log(typeof console.finishAsyncTask);
    console.log(typeof console.cancelAsyncTask);
  `;

  for (const contextId of configs) {
    await dp.Runtime.evaluate({expression: code, contextId});
  }

  testRunner.completeTest();
});
