(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`This tests how navigation is handled from inside debugger code (console.log).`);
  setTimeout(()=>testRunner.completeTest(), 3000);
  await dp.Runtime.enable();
  await checkExpression('logArray()');
  await checkExpression('logDate()');
  await checkExpression('logDateWithArg()');

  async function checkExpression(expression) {
    const iframeUrl = testRunner.url('../resources/console-log-navigate.html');
    const iframeContextCreated = dp.Runtime.onceExecutionContextCreated();
    await session.evaluateAsync(`
      new Promise(resolve => {
        let frame = document.createElement('iframe');
        frame.src = "${iframeUrl}";
        frame.onload = resolve;
        document.body.appendChild(frame);
      });
    `);
    const contextId = (await iframeContextCreated).params.context.id;

    testRunner.log(`Got new context: ${contextId !== undefined}`);

    const aboutBlankContextCreated = dp.Runtime.onceExecutionContextCreated();
    testRunner.log(await dp.Runtime.evaluate({
      expression: expression,
      contextId: contextId
    }));
    await aboutBlankContextCreated;
  }

})
