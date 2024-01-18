(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that stepping over caught exception does not pause.');

  dp.Debugger.enable();
  dp.Runtime.enable();
  dp.Runtime.evaluate({expression: `





    function testFunction() {
      function foo() {
        try {
            throw new Error();
        } catch (e) {
        }
      }
      debugger;
      foo();
      console.log('completed');
    }
    setTimeout(testFunction, 0);
  `});

  function printCallFrames(messageObject) {
    var callFrames = messageObject.params.callFrames;
    for (var callFrame of callFrames)
      testRunner.log(callFrame.functionName + ':' + callFrame.location.lineNumber);
  }

  printCallFrames(await dp.Debugger.oncePaused());
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  dp.Debugger.stepOver();
  printCallFrames(await dp.Debugger.oncePaused());
  dp.Debugger.resume();
  await dp.Runtime.onceConsoleAPICalled(messageObject => messageObject.params.args[0].value === 'completed');

  dp.Runtime.evaluate({expression: 'setTimeout(testFunction, 0);'} );
  printCallFrames(await dp.Debugger.oncePaused());
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  dp.Debugger.stepInto();
  await dp.Debugger.oncePaused();
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  dp.Debugger.stepOver();
  printCallFrames(await dp.Debugger.oncePaused());
  dp.Debugger.resume();
  await dp.Runtime.onceConsoleAPICalled(messageObject => messageObject.params.args[0].value === 'completed');

  testRunner.completeTest();
})
