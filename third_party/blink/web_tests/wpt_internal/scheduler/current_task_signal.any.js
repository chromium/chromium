// META: title=Scheduling API: Signal inheritance
// META: global=window
'use strict';

async_test(t => {
  var result = "fail";
  let tc = new TaskController("user-blocking");
  scheduler.postTask(() => {
    scheduler.postTask(() => {
      assert_equals(scheduler.currentTaskSignal.priority, "user-blocking");
      result = "pass";
    }, { signal: scheduler.currentTaskSignal });
  }, { signal: tc.signal });

  // Since the above tasks should be run at high priority, it should execute
  // before this default priority task.
  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, "pass");
  }));
}, 'Test that currentTaskSignal uses the incumbent priority');
