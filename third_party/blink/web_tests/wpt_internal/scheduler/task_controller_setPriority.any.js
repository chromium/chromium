// META: title=Scheduling API: TaskController.setPriority()
// META: global=window
'use strict';

async_test(t => {
  let result = '';
  let tc = new TaskController("user-visible");

  for (let i = 0; i < 5; i++) {
    let task = scheduler.postTask(() => {
      result += i.toString();
    }, { signal: tc.signal });
  }

  scheduler.postTask(() => { result += "5"; }, { priority : "user-blocking" });
  scheduler.postTask(() => { result += "6"; }, { priority : "user-visible" });

  tc.setPriority("background");

  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, '5601234');
  }), { priority: "background" });

}, 'Test that TaskController.setPriority() changes the priority of all associated tasks');
