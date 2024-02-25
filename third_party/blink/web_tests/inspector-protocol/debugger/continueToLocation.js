(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests continueToLocation functionality.');

  function statementsExample()
  {
      var self = arguments.callee;

      debugger;

      self.step = 1;

      self.step = 2;

      void [
          self.step = 3,
          self.step = 4,
          self.step = 5,
          self.step = 6
      ];

      self.step = 7;
  }

  var scenarios = [
      // requested line number, expected control parameter 'step', expected line number
      [ 8, 1, 8 ],
      [ 8, 1, 8 ],
      [ 12, 6, 17 ],
      [ 13, 6, 17 ],
      [ 17, 6, 17 ],
      [ 17, 6, 17 ],
  ];

  dp.Debugger.enable();
  var functionResponse = await dp.Runtime.evaluate({expression: statementsExample.toString() + '; statementsExample'});
  var functionObjectId = functionResponse.result.result.objectId;

  var detailsResponse = await dp.Runtime.getProperties({objectId: functionObjectId});
  var scriptId;
  for (var prop of detailsResponse.result.internalProperties) {
    if (prop.name === '[[FunctionLocation]]')
      scriptId = prop.value.value.scriptId;
  }

  for (var scenario of scenarios) {
    var lineNumber = scenario[0];
    var expectedResult = scenario[1];
    var expectedLineNumber = scenario[2];
    dp.Runtime.evaluate({expression: 'setTimeout(statementsExample, 0)' });
    await dp.Debugger.oncePaused();
    testRunner.log('Paused on debugger statement');

    var continueToLocationResponse = await dp.Debugger.continueToLocation({location: {scriptId, lineNumber, columnNumber: 0}});
    if (continueToLocationResponse.error) {
      testRunner.log('Failed to execute continueToLocation ' + JSON.stringify(continueToLocationResponse.error));
      testRunner.completeTest();
      return;
    }

    var messageObject = await dp.Debugger.oncePaused();
    testRunner.log('Paused after continueToLocation');
    var actualLineNumber = messageObject.params.callFrames[0].location.lineNumber;
    testRunner.log('Stopped on line ' + actualLineNumber + ', expected ' + expectedLineNumber + ', requested ' + lineNumber + ', (0-based numbers).');

    dp.Debugger.onPaused(handleDebuggerPausedUnexpected);
    var resultValue = (await dp.Runtime.evaluate({expression: 'statementsExample.step' })).result.result.value;
    testRunner.log(`Control parameter 'step' calculation result: ${resultValue}, expected: ${expectedResult}`);
    testRunner.log(resultValue == expectedResult ? 'SUCCESS' : 'FAIL');
    dp.Debugger.resume();
    dp.Debugger.offPaused(handleDebuggerPausedUnexpected);

    function handleDebuggerPausedUnexpected() {
      testRunner.log('Unexpected debugger pause');
      testRunner.completeTest();
    }
  }

  testRunner.completeTest();
})
