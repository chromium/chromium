(async function (testRunner) {
  var { page, session, dp } = await testRunner.startBlank(`Tests execution context lifetime events.`);

  dp.Runtime.enable();
  await dp.Runtime.onceExecutionContextCreated();
  testRunner.log('Page context was created');

  testRunner.log('Create new frame');
  var loadPromise = session.evaluateAsync(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    frame.id = 'iframe';
    document.body.appendChild(frame);
    new Promise(resolve => frame.onload = resolve)
  `);
  var frameExecutionContextId = (await dp.Runtime.onceExecutionContextCreated()).params.context.id;
  testRunner.log('Frame context was created');

  await loadPromise;
  testRunner.log('Navigate frame');
  session.evaluate(`
    window.frames[0].location = '${testRunner.url('../resources/runtime-events-iframe.html')}'
    GCController.collectAll();
  `);
  var executionContextId = (await dp.Runtime.onceExecutionContextDestroyed()).params.executionContextId;
  if (frameExecutionContextId !== executionContextId) {
    testRunner.fail(`Execution context with id = ${executionContextId} was destroyed, but iframe's executionContext had id = ${frameExecutionContextId} before navigation`);
    return;
  }
  testRunner.log(`Frame's context was destroyed`);
  frameExecutionContextId = 0;

  frameExecutionContextId = (await dp.Runtime.onceExecutionContextCreated()).params.context.id;
  testRunner.log('Frame context was created');

  testRunner.log('Remove frame');
  session.evaluate(`
    document.querySelector('#iframe').remove();
    GCController.collectAll();
  `);
  executionContextId = (await dp.Runtime.onceExecutionContextDestroyed()).params.executionContextId;
  if (frameExecutionContextId !== executionContextId) {
    testRunner.fail(`Deleted frame had execution context with id = ${frameExecutionContextId}, but executionContext with id = ${executionContextId} was removed`);
    return;
  }
  testRunner.log(`Frame's context was destroyed`);

  testRunner.log('Create new crafted frame');
  session.evaluate(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    frame.id = 'crafted-iframe';
    document.body.appendChild(frame);
    frame.contentDocument.write('<div>crafted</div>');
    frame.contentDocument.close();
  `);

  frameExecutionContextId = (await dp.Runtime.onceExecutionContextCreated()).params.context.id;
  testRunner.log('Crafted frame context was created');

  testRunner.log('Remove crafted frame');
  session.evaluate(`
    document.querySelector('#crafted-iframe').remove();
    GCController.collectAll();
  `);
  executionContextId = (await dp.Runtime.onceExecutionContextDestroyed()).params.executionContextId;
  if (frameExecutionContextId !== executionContextId) {
    testRunner.fail(`Deleted frame had execution context with id = ${frameExecutionContextId}, but executionContext with id = ${executionContextId} was removed`);
    return;
  }
  testRunner.log(`Crafted frame's context was destroyed`);
  testRunner.completeTest();
})
