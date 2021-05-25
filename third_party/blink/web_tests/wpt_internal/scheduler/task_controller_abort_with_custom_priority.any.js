// META: title=Scheduling API: TaskController.abort()
// META: global=window
'use strict';

async_test(t => {
  let result = 0;
  let tc = new TaskController();

  scheduler.postTask(() => {}, { signal: tc.signal }).then(
      () => { assert_unreached('This task should have been aborted'); },
      () => { result++; });
  scheduler.postTask(() => {}, { priority: "background", signal: tc.signal }).then(
      () => { assert_unreached('This task should have been aborted'); },
      () => { result++; });
  tc.abort();

  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, 2);
  }), { priority: "background" });

}, 'Test that when scheduler.postTask() is given both a signal and priority. the signal abort is honored');
