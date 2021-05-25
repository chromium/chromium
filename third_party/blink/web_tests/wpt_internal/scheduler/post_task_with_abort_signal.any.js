// META: title=Scheduling API: postTask and AbortSignal
// META: global=window
'use strict';

async_test(t => {
  let result = "fail";
  let ac = new AbortController;
  scheduler.postTask(() => {}, { signal: ac.signal }).then(
      () => { assert_unreached('This task should have been aborted'); },
      () => { result = "pass"; });
  ac.abort();

  // The task should be aborted.
  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, "pass");
  }));
}, 'Test that scheduler.postTask() accepts an AbortSignal that is not also a TaskSignal');
