(async function(testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests that async stack tagging API (recurring) works as expected.');

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
  /* --- Runtime --- */

  // Something akin to requestIdleCallback, but deterministic.

  const BUDGET = 2;

  function fakeRequestIdleCallback(cb) {
    function makeDeadline() {
      let ticks = BUDGET;

      return {
        ticksRemaining() {
          return ticks--;
        },
      };
    }

    cb(makeDeadline());
  }

  /* --- Library --- */

  function makeTask(id, name, jobs) {
    return {
      runNextJob() {
        const f = jobs.shift();
        console.startAsyncTask(id);
        f();
        console.finishAsyncTask(id);

        if (jobs.length) {
          return false;
        }

        console.cancelAsyncTask(id);
        return true;
      },
    };
  }

  function makeScheduler() {
    const tasks = [];

    function workLoop(deadline) {
      while (deadline.ticksRemaining() > 0 && tasks.length) {
        const task = tasks[0];
        const finished = task.runNextJob();
        if (finished) {
          tasks.shift();
        }
      }

      if (tasks.length) {
        fakeRequestIdleCallback(workLoop);
        return;
      }

    }

    return {
      scheduleTask(name, jobs) {
        const id = console.scheduleAsyncTask(name, true);
        tasks.push(makeTask(id, name, jobs));
      },

      scheduleWorkLoop() {
        fakeRequestIdleCallback(workLoop);
      },
    };
  }

  /* --- Userland --- */

  function makeJob(i) {
    return function someJob() {
      console.trace(\`completeWork: job \${i}\`);
    };
  }

  const scheduler = makeScheduler();

  function businessLogic() {
    scheduler.scheduleTask("fooTask", [makeJob(1), makeJob(2), makeJob(3)]);
    scheduler.scheduleTask("barBask", [makeJob(4), makeJob(5)]);
  }

  businessLogic();
  scheduler.scheduleWorkLoop();
  `;

  for (const contextId of configs) {
    await dp.Runtime.evaluate({expression: code, contextId});
  }

  testRunner.completeTest();
});
