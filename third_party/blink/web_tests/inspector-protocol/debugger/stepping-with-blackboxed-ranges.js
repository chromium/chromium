(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests how stepping works with some source ranges blackboxed.');

  function printCallFrames(response) {
    var callFrames = response.params.callFrames;
    var topCallFrame = callFrames[0];
    if (topCallFrame.functionName.startsWith('blackboxed'))
      testRunner.log('FAIL: blackboxed function in top call frame');
    for (var callFrame of callFrames)
      testRunner.log(callFrame.functionName + ': ' + callFrame.location.lineNumber + ':' + callFrame.location.columnNumber);
    testRunner.log('');
  }

  function printError(response) {
    if (response.error)
      testRunner.log(response.error.message);
  }

  await session.evaluate(
`function blackboxedBoo()
{
    var a = 42;
    var b = foo();
    return a + b;
}
//# sourceURL=blackboxed-script.js
`);

  await session.evaluate(
`function notBlackboxedFoo()
{
    var a = 42;
    var b = blackboxedBoo();
    return a + b;
}

function blackboxedFoo()
{
    var a = 42;
    var b = notBlackboxedFoo();
    return a + b;
}

function notBlackboxedBoo()
{
    var a = 42;
    var b = blackboxedFoo();
    return a + b;
}
//# sourceURL=mixed-source.js
`);

  await session.evaluate(`





function testFunction()
{
    notBlackboxedBoo(); // for setup ranges and stepOut
    notBlackboxedBoo(); // for stepIn
}

function foo()
{
    debugger;
    return 239;
}
  `);

  await dp.Debugger.enable();
  session.evaluate('setTimeout(testFunction, 0);');

  var response = await dp.Debugger.oncePaused();
  printCallFrames(response);
  var scriptId = response.params.callFrames[2].location.scriptId;

  printError(await dp.Debugger.setBlackboxedRanges({
    scriptId: response.params.callFrames[1].location.scriptId,
    positions: [{lineNumber: 0, columnNumber: 0}] // blackbox ranges for blackboxed.js
  }));

  var incorrectPositions = [
    [{lineNumber: 0, columnNumber: 0}, {lineNumber: 0, columnNumber: 0}],
    [{lineNumber: 0, columnNumber: 1}, {lineNumber: 0, columnNumber: 0}],
    [{lineNumber: 0, columnNumber: -1}],
  ];
  for (var positions of incorrectPositions) {
    testRunner.log('Try to set positions: ' + JSON.stringify(positions));
    printError(await dp.Debugger.setBlackboxedRanges({scriptId, positions}));
  }

  await dp.Debugger.setBlackboxedRanges({
    scriptId,
    positions: [{lineNumber: 6, columnNumber: 0}, {lineNumber: 14, columnNumber: 0}] // blackbox ranges for mixed.js
  });

  testRunner.log('action: stepOut');
  dp.Debugger.stepOut();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepOut');
  dp.Debugger.stepOut();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepOut');
  dp.Debugger.stepOut();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepInto');
  dp.Debugger.stepInto();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepOver');
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  testRunner.log('action: stepInto');
  dp.Debugger.stepInto();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepOver');
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  testRunner.log('action: stepInto');
  dp.Debugger.stepInto();
  printCallFrames(await dp.Debugger.oncePaused());
  testRunner.log('action: stepOver');
  dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();
  testRunner.log('action: stepInto');
  dp.Debugger.stepInto();
  printCallFrames(await dp.Debugger.oncePaused());
  await dp.Debugger.resume();
  testRunner.completeTest();
})
