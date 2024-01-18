(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank('Tests that an infinite loop can be interrupted and paused from a step-over.');

  await dp.Debugger.enable();

  dp.Runtime.evaluate({
    expression: `
      function f() {
        let i = 0;
        while (true) {i++}
      }

      debugger;
      f();`,
  });

  testRunner.log('didEval');
  await dp.Debugger.oncePaused();
  dp.Debugger.stepInto();
  await dp.Debugger.oncePaused();

  testRunner.log('Expecting the debugger to pause the loop ...');
  await dp.Debugger.stepOver();

  // Delay the pause slightly to let the loop do some spinning.
  await new Promise(r => setTimeout(r, 0));

  dp.Debugger.pause();
  await dp.Debugger.oncePaused();

  testRunner.log('SUCCESS');
  testRunner.completeTest();
});
