// META: title=Scheduling API: Move Delayed Tasks
// META: global=window

'use strict';

let taskCount = 0;
let taskToMove;

async_test(t => {
  let now = performance.now();

  let tc = new TaskController('background');

  scheduler.postTask(t.step_func(() => {
    assert_equals(++taskCount, 1);
    tc.setPriority('user-blocking');
  }), { priority: 'user-blocking', delay: 10 });

  scheduler.postTask(t.step_func_done(() => {
    assert_equals(++taskCount, 2);

    let elapsed = performance.now() - now;
    assert_greater_than_equal(elapsed, 20);
  }), { signal: tc.signal, delay: 20 });

}, "Tests delay when changing a delayed task's priority");
