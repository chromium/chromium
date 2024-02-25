(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { page, session, dp } = await testRunner.startBlank(`Tests execution context lifetime events.`);

  const expectedDestroyedExecutionContexts = new Set();
  const actualDestroyedExecutionContexts = new Set();
  let allContextsCreated = false;
  // The promise will resolve when the 3 created execution contexts are destroyed.
  // Given that the timing is GC dependent, we only validate that at the end of the
  // test we received a destroyed event for each context.
  // The prevent the promise from resolving prematurely, we have to wait until
  // all execution contexts have been created first.
  const allContextsDestroyedPromise = new Promise(resolve => {
    dp.Runtime.onExecutionContextDestroyed(event => {
      actualDestroyedExecutionContexts.add(event.params.executionContextId);

      if (!allContextsCreated) return;

      for (const id of expectedDestroyedExecutionContexts) {
        if (!actualDestroyedExecutionContexts.has(id)) return;
      }

      resolve();
    });
  });


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
  expectedDestroyedExecutionContexts.add(frameExecutionContextId);
  testRunner.log('Frame context was created');

  await loadPromise;
  testRunner.log('Navigate frame');
  var loadPromise = session.evaluateAsync(`
    var frame = document.querySelector('#iframe');
    frame.contentWindow.location = '${testRunner.url('../resources/runtime-events-iframe.html')}';
    new Promise(resolve => frame.onload = resolve)
  `);
  frameExecutionContextId = 0;

  frameExecutionContextId = (await dp.Runtime.onceExecutionContextCreated()).params.context.id;
  expectedDestroyedExecutionContexts.add(frameExecutionContextId);
  testRunner.log('Frame context was created');

  await loadPromise;
  testRunner.log('Remove frame');
  session.evaluate(`document.querySelector('#iframe').remove();`);

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
  expectedDestroyedExecutionContexts.add(frameExecutionContextId);
  allContextsCreated = true;
  testRunner.log('Crafted frame context was created');

  testRunner.log('Remove crafted frame');
  session.evaluate(`
    document.querySelector('#crafted-iframe').remove();
    GCController.collectAll();
  `);

  await allContextsDestroyedPromise;

  testRunner.log(`All contexts destroyed!`);
  testRunner.completeTest();
})
