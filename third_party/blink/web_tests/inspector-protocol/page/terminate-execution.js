(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startHTML(`
    <div></div>
    <script>
      document.querySelector('div').addEventListener('click', () => { console.log(42); while(true){} });
      document.querySelector('div').addEventListener('click', () => { while(true){} });
    </script>
  `, 'Tests terminate execution.');
  dp.Runtime.enable();
  let promise =
      dp.Runtime.evaluate({expression: `document.querySelector('div').click()`})
          .then(msg => testRunner.log(msg));
  await dp.Runtime.onceConsoleAPICalled();
  await Promise.all([
    dp.Emulation.setScriptExecutionDisabled({value: true}),
    dp.Runtime.terminateExecution().then(msg => testRunner.log(msg)), promise
  ]);
  testRunner.completeTest();
})
