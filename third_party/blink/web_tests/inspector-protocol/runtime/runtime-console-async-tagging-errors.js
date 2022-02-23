(async function(testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests that async stack tagging API errors out as expected.');

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

  const configs = [pageContextId, frameContextId];

  dp.Runtime.onConsoleAPICalled((result) => testRunner.log(result));
  dp.Runtime.onExceptionThrown((result) => testRunner.log(result));

  const apiEnabled = await dp.Runtime.evaluate(
      {expression: `"scheduleAsyncTask" in console`, contextId: pageContextId});
  if (!apiEnabled.result.result.value) {
    testRunner.log('Skipping: async stack tagging API not enabled.');
    testRunner.completeTest();
    return;
  }

  const failsScheduleWithNotEnoughArgs = {
    name: 'Scheduling an async task with no argument should fail',
    code: `
      console.scheduleAsyncTask();
    `
  };

  const failsScheduleWithBadRecurringArg = {
    name: 'Scheduling an async task with a bad recurring flag should fail',
    code: `
      console.scheduleAsyncTask("foo", 42);
    `
  };

  const failsScheduleWithTooManyArgs = {
    name: 'Scheduling an async task with too many arguments should fail',
    code: `
      console.scheduleAsyncTask("foo", false, 42);
    `
  };

  const failsStartWithBadTaskId = {
    name: 'Starting an async task with an non-integer task id should fail',
    code: `
      console.startAsyncTask("wat");
    `
  };

  const failsStartWithWrongTaskId = {
    name: 'Starting an async task with an inexistent task id should fail',
    code: `
      console.startAsyncTask(100042);
    `
  };

  const failsStopWithBadTaskId = {
    name: 'Stopping an async task with an non-integer task id should fail',
    code: `
      console.stopAsyncTask("wat");
    `
  };

  const failsStopWithWrongTaskId = {
    name: 'Stopping an async task with an inexistent task id should fail',
    code: `
      console.stopAsyncTask(100042);
    `
  };

  const failsCancelWithBadTaskId = {
    name: 'Cancelling an async task with an non-integer task id should fail',
    code: `
      console.cancelAsyncTask("wat");
    `
  };

  const failsCancelWithWrongTaskId = {
    name: 'Cancelling an async task with an inexistent task id should fail',
    code: `
      console.cancelAsyncTask(100042);
    `
  };

  const failsToStartAfterCancel = {
    name: 'Starting a cancelled async task should fail',
    code: `
      let id = console.scheduleAsyncTask("cancel me");
      console.log(id);
      console.cancelAsyncTask(id);
      console.startAsyncTask(id);
    `
  };

  const failsToFinishAfterCancel = {
    name: 'Finishing a cancelled async task should fail',
    code: `
      let id = console.scheduleAsyncTask("cancel me");
      console.log(id);
      console.startAsyncTask(id);
      console.cancelAsyncTask(id);
      console.finishAsyncTask(id);
    `
  };

  const failsToCancelAfterFinish = {
    name: 'Cancelling an already finished async task should fail',
    code: `
      let id = console.scheduleAsyncTask("cancel me");
      console.log(id);
      console.startAsyncTask(id);
      console.finishAsyncTask(id);
      console.cancelAsyncTask(id);
    `
  };

  const failsToCancelAfterCancel = {
    name: 'Cancelling an already cancelled async task should fail',
    code: `
      let id = console.scheduleAsyncTask("cancel me");
      console.log(id);
      console.cancelAsyncTask(id);
      console.cancelAsyncTask(id);
    `
  };

  const checks = [
    failsScheduleWithNotEnoughArgs,
    failsScheduleWithBadRecurringArg,
    failsScheduleWithTooManyArgs,
    failsStartWithBadTaskId,
    failsStartWithWrongTaskId,
    failsStopWithBadTaskId,
    failsCancelWithBadTaskId,
    failsCancelWithWrongTaskId,
    failsStopWithWrongTaskId,
    failsToStartAfterCancel,
    failsToFinishAfterCancel,
    failsToCancelAfterFinish,
    failsToCancelAfterCancel,
  ];

  for (const contextId of configs) {
    for (const {name, code} of checks) {
      testRunner.log(name);
      await dp.Runtime.evaluate({
        expression: `try { ${code} } catch (e) { console.log(e.message); }`,
        contextId
      });
    }
  }

  testRunner.completeTest();
});
