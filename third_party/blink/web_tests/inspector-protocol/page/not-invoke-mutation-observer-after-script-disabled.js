(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests mutation observer invocation.');

  await dp.Runtime.enable();
  let promise = page.navigate('../resources/mutation-observer-triggered-by-parser.html')

  await dp.Runtime.onceConsoleAPICalled();
  await Promise.all([
    dp.Emulation.setScriptExecutionDisabled({value: true}),
    dp.Runtime.terminateExecution(),
    promise
  ]);

  let result = await session.evaluate(`count`);
  testRunner.log(`Count: ${result}.`);

  testRunner.completeTest();
})
