(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests setScriptSource functionality.');

  function logEqualsCheck(actual, expected) {
    if (actual == expected) {
      testRunner.log('PASS, result value: ' + actual);
    } else {
      testRunner.log('FAIL, actual value: ' + actual + ', expected: ' + expected);
    }
  }

  await session.evaluate(
    `function TestExpression(a, b) {
      return a + b;
    }`);

  await dp.Debugger.enable();

  var response = await dp.Runtime.evaluate({expression: 'TestExpression(2, 4)' });
  testRunner.log('Function evaluate: ' + JSON.stringify(response.result.result));
  logEqualsCheck(response.result.result.value, 6);

  var functionObjectId = (await dp.Runtime.evaluate({expression: 'TestExpression' })).result.result.objectId;
  var result = (await dp.Runtime.getProperties({ objectId: functionObjectId})).result;
  var scriptId;
  for (var prop of result.internalProperties) {
    if (prop.name === '[[FunctionLocation]]')
      scriptId = prop.value.value.scriptId;
  }

  var originalText = (await dp.Debugger.getScriptSource({scriptId})).result.scriptSource;
  var patched = originalText.replace('a + b', 'a * b');
  await dp.Debugger.setScriptSource({scriptId, scriptSource: patched});

  var response = await dp.Runtime.evaluate({expression: 'TestExpression(2, 4)' });
  testRunner.log('Function evaluate: ' + JSON.stringify(response.result.result));
  logEqualsCheck(response.result.result.value, 8);

  patched = originalText.replace('a + b', 'a # b');
  var exceptionDetails = (await dp.Debugger.setScriptSource({scriptId, scriptSource: patched})).result.exceptionDetails;
  testRunner.log(`Has error reported: ${!!exceptionDetails ? 'PASS' : 'FAIL'}`, );
  testRunner.log(`Reported error is a compile error: ${!!exceptionDetails ? 'PASS' : 'FAIL'}`, );
  if (exceptionDetails)
    logEqualsCheck(exceptionDetails.lineNumber, 1);
  testRunner.completeTest();
})
