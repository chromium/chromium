(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Check that stepInto at then end of the script go to next user script instead InjectedScriptSource.js.');

  function logStack(event) {
    testRunner.log('Stack trace:');
    for (var callFrame of event.params.callFrames)
      testRunner.log(callFrame.functionName + ':' + callFrame.location.lineNumber + ':' + callFrame.location.columnNumber);
    testRunner.log('');
  }

  dp.Debugger.enable();
  await session.evaluate(`





  function foo() {
    return 239;
  }`);
  dp.Runtime.evaluate({expression: '(function boo() { setTimeout(foo, 0); debugger; })()' });

  logStack(await dp.Debugger.oncePaused());
  testRunner.log('Perform stepInto');
  dp.Debugger.stepInto();

  logStack(await dp.Debugger.oncePaused());
  testRunner.log('Perform stepInto');
  dp.Debugger.stepInto();

  logStack(await dp.Debugger.oncePaused());
  testRunner.log('Perform stepInto');
  dp.Debugger.stepInto();

  logStack(await dp.Debugger.oncePaused());
  await dp.Debugger.resume();
  testRunner.completeTest();
})
