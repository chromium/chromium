// META: title=Scheduling API: Task.result Accessed After Task Runs
// META: global=window
'use strict';

async_test(t => {
  (function() {
    let task_promise = scheduler.postTask(t.step_func(() => {
      // This task will run after |task| finishes.
      scheduler.postTask(t.step_func(() => {
        task_promise.then(t.step_func_done((res) => {
          assert_equals(res, 1234);
        }));
      }));
      return 1234;
    }));
  })();

}, 'Test task result is resolved properly when accessed after the task runs');
