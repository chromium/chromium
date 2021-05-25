// META: title=Scheduling API: Post Delayed Tasks
// META: global=window
'use strict';

async_test(t => {
  let now = performance.now();
    scheduler.postTask(t.step_func_done(() => {
      let elapsed = performance.now() - now;
      assert_greater_than_equal(elapsed, 10);
    }), { priority: 'user-blocking', delay: 10 });
}, 'Tests basic scheduler.postTask with a delay');
