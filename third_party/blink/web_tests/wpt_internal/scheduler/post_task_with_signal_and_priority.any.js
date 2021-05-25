// META: title=Scheduling API: Global Task Queues
// META: global=window
'use strict';

async_test(t => {
  var result = "fail";
  let tc = new TaskController("background");
  scheduler.postTask(() => { result = "pass"; }, { priority : "user-blocking", signal: tc.signal });

  // Since the above task should be run at user-blocking priority, it should execute
  // before this user-visible priority task.
  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, "pass");
  }));
}, 'Test when scheduler.postTask() is passed both a signal and a priority');
