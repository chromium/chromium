(async function(testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests that async stack tagging API deals with collisions as expected.');

  dp.Runtime.enable();

  const response1 = await dp.Runtime.onceExecutionContextCreated();
  const pageContextId = response1.params.context.id;  // main page
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  const response2 = await dp.Runtime.onceExecutionContextCreated();
  const frameContextId = response2.params.context.id;  // IFrame

  const apiEnabled = await dp.Runtime.evaluate(
      {expression: `"scheduleAsyncTask" in console`, contextId: pageContextId});
  if (!apiEnabled.result.result.value) {
    testRunner.log('Skipping: async stack tagging API not enabled.');
    testRunner.completeTest();
    return;
  }

  const configs = [pageContextId, frameContextId];

  dp.Runtime.onConsoleAPICalled((result) => testRunner.log(result));
  dp.Runtime.onExceptionThrown((result) => testRunner.log(result));

  const canReuseName = {
    name: 'Scheduling async tasks with the same name should make different ids',
    code: `
      let id1 = console.scheduleAsyncTask("foo");
      console.log(id1);
      let id2 = console.scheduleAsyncTask("foo");
      console.log(id2); // should not equal to id1
      // cleanup
      console.cancelAsyncTask(id1);
      console.cancelAsyncTask(id2);
      // check
      id1 !== id2
    `
  };

  const doesNotReuseIds = {
    name: 'Cancelling async tasks does not end up reusing task ids',
    code: `
      let id1 = console.scheduleAsyncTask("A");
      console.log(id1);
      console.cancelAsyncTask(id1);
      let id2 = console.scheduleAsyncTask("B");
      console.log(id2); // should not equal to id1
      console.cancelAsyncTask(id2);
      // check
      id1 !== id2
    `
  };

  const doesNotCollideIds = {
    name: 'Cancelling async tasks does not end up colliding task ids',
    code: `
      let id1 = console.scheduleAsyncTask("A");
      console.log(id1);
      let id2 = console.scheduleAsyncTask("B");
      console.log(id2);
      console.cancelAsyncTask(id1);
      let id3 = console.scheduleAsyncTask("C");
      console.log(id3); // should not equal to id2
      // cleanup
      console.cancelAsyncTask(id2);
      console.cancelAsyncTask(id3);
      // check
      id1 !== id2 && id2 !== id3
    `
  };

  const checks = [
    canReuseName,
    doesNotReuseIds,
    doesNotCollideIds,
  ];

  for (const contextId of configs) {
    for (const {name, code} of checks) {
      testRunner.log(name);
      let check =
          await dp.Runtime.evaluate({expression: `{${code}}`, contextId});
      if (!check.result.result.value) {
        testRunner.fail(`Failed check: ${name}`);
      }
    }
  }

  testRunner.completeTest();
});
