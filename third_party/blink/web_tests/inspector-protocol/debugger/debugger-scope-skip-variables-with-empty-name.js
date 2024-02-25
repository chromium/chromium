(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that we do not report scope variables with empty names.');

  dp.Debugger.enable();
  dp.Runtime.evaluate({expression: `
    function testFunction() {
      for (var a of [1]) {
        ++a;
        debugger;
      }
    }
    testFunction();
  `});

  var message = await dp.Debugger.oncePaused();
  var scopeChain = message.params.callFrames[0].scopeChain;
  var localScopeObjectIds = [];
  for (var scope of scopeChain) {
    if (scope.type === 'local')
      localScopeObjectIds.push(scope.object.objectId);
  }

  for (var objectId of localScopeObjectIds)
    testRunner.log((await dp.Runtime.getProperties({objectId})).result);

  await dp.Debugger.resume();
  testRunner.completeTest();
})
