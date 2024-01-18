(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that evaluation on call frame affects scope variables.');

  var newVariableValue = 55;

  dp.Debugger.enable();
  dp.Runtime.evaluate({expression: `
    function TestFunction() {
      var a = 2;
      debugger;
      debugger;
    }
    setTimeout(TestFunction, 0);
  `});

  var messageObject = await dp.Debugger.oncePaused();
  testRunner.log(`Paused on 'debugger;'`);
  var topFrame = messageObject.params.callFrames[0];
  var topFrameId = topFrame.callFrameId;

  await dp.Debugger.evaluateOnCallFrame({callFrameId: topFrameId, expression: 'a = ' + newVariableValue });
  testRunner.log('Variable value changed');
  dp.Debugger.resume();

  var response = await dp.Debugger.oncePaused();
  testRunner.log('Stacktrace re-read again');
  var localScope = response.params.callFrames[0].scopeChain[0];

  response = await dp.Runtime.getProperties({objectId: localScope.object.objectId });
  testRunner.log('Scope variables downloaded anew');
  var propertyList = response.result.result;
  var varNamedA = propertyList.find(x => x.name === 'a');
  if (varNamedA) {
    var actualValue = varNamedA.value.value;
    testRunner.log('New variable is ' + actualValue + ', expected is ' + newVariableValue + ', old was: 2');
    testRunner.log(actualValue == newVariableValue ? 'SUCCESS' : 'FAIL');
  } else {
    testRunner.log('Failed to find variable in scope');
  }
  testRunner.completeTest();
})
