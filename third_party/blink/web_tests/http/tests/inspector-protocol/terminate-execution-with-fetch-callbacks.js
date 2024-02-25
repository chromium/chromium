(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests terminate execution.');
  dp.Runtime.enable();
  let consoleCall = dp.Runtime.onceConsoleAPICalled();
  dp.Runtime.evaluate({
    expression: `let p = fetch('data:text/plain,Hello again!');
    p.then(() => {console.log(1); while(true){}});
    p.then(() => console.log(42));`
  });
  await consoleCall.then(msg => testRunner.log(msg.params.args[0]));
  await Promise.all([
    dp.Runtime
        .evaluate({expression: '\'evaluated after\'', returnByValue: true})
        .then(msg => testRunner.log(msg)),
    dp.Runtime.terminateExecution().then(msg => testRunner.log(msg))
  ]);
  testRunner.completeTest();
})
