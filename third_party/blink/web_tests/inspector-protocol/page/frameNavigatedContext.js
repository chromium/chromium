(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { page, session, dp } = await testRunner.startBlank(`Tests context lifetime events relative to frame's ones.`);

  await dp.Runtime.enable();
  await dp.Page.enable();
  var frameId;
  var contextId;
  dp.Page.onFrameNavigated(result => {
    frameId = result.params.frame.id;
    testRunner.log('Frame navigated')
  });
  dp.Runtime.onExecutionContextCreated(result => {
    const contextFrameId = result.params.context.auxData.frameId;
    contextId = result.params.context.id;
    testRunner.log('Context created');
    testRunner.log('Frame id is matching: ' + (frameId === contextFrameId));
  });
  dp.Runtime.onExecutionContextDestroyed(result => {
    testRunner.log('Context destroyed');
    testRunner.log('Destroyed context id is matching: ' + (result.params.executionContextId === contextId));
  });

  testRunner.log('\nLoading iframe');
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);

  `);

  await dp.Runtime.onceExecutionContextCreated();

  testRunner.log('\nNavigating iframe');
  session.evaluate(`
    window.frame.src = '${testRunner.url('../resources/blank.html?loadAgain')}';
  `);

  await dp.Runtime.onceExecutionContextCreated();

  testRunner.log('\nUnloading iframe');
  session.evaluate(`
    frame.remove();
    GCController.collectAll();
  `);

  await dp.Runtime.onceExecutionContextDestroyed();

  testRunner.completeTest();
})
