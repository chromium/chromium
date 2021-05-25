// META: title=Scheduling API: Global Task Queues
// META: global=window
'use strict';

async_test(t => {
  function* priorityGenerator() {
    let priorities = [
      "user-blocking", "user-visible", "background"
    ];
    for (let i = 0; i < priorities.length; i++)
      yield priorities[i];
  }

  function testPriority(priority) {
    let task = scheduler.postTask(t.step_func(() => {
      nextTaskQueue();
    }), { priority: priority });
  }

  let nextPriority = priorityGenerator();

  function nextTaskQueue() {
    let next = nextPriority.next();
    if (next.done) {
      t.done();
      return;
    }
    testPriority(next.value);
  }

  // Schedule a task to kick things off.
  scheduler.postTask(t.step_func(() => {
    nextTaskQueue();
  }));
}, 'Basic functionality for scheduler.postTask() without using a task signal');
