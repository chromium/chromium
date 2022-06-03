(async function(testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests that async stack tagging API works as expected.');

  dp.Runtime.enable();
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

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

  const code = `
  /* --- Library --- */

  function makeScheduler() {
    let stack = [];

    return {
      scheduleUnitOfWork(f) {
        const id = console.scheduleAsyncTask(f.name);
        stack.push({ id, f });
      },

      workLoop() {
        while (stack.length) {
          const { id, f } = stack.pop();
          console.startAsyncTask(id);
          f();
          console.finishAsyncTask(id);
        }
      },
    };
  }

  /* --- Userland --- */

  const scheduler = makeScheduler();

  function someTask() {
    console.trace("completeWork: someTask");
  }

  function someOtherTask() {
    console.trace("completeWork: someOtherTask");
  }

  function businessLogic() {
    scheduler.scheduleUnitOfWork(someTask);
    scheduler.scheduleUnitOfWork(someOtherTask);
  }

  businessLogic();
  scheduler.workLoop();
  `;

  for (const contextId of configs) {
    await dp.Runtime.evaluate({expression: code, contextId});
  }

  testRunner.completeTest();
});
