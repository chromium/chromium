(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests call frames in isolated worlds.');

  var response = await dp.Page.getResourceTree();
  var mainFrameId = response.result.frameTree.frame.id;
  testRunner.log('Main frame obtained');

  response = await dp.Page.createIsolatedWorld({frameId: mainFrameId, worldName: 'Test world'});
  var contextId = response.result.executionContextId;
  testRunner.log('Isolated world created');

  await dp.Runtime.evaluate({executionContextId: contextId, expression: `function func1() { debugger; }`});
  await dp.Debugger.enable();
  testRunner.log('Pausing');
  dp.Runtime.evaluate({executionContextId: contextId, expression: `function func2() { func1(); }; func2();`});
  response = await dp.Debugger.oncePaused();
  testRunner.log(`Paused, call frames count: ${response.params.callFrames.length}`);
  await dp.Debugger.resume();
  testRunner.log('Resumed');

  testRunner.completeTest();
})
