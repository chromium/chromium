// META: title=Scheduling API: Recursive TaskController.setPriority()
// META: global=window
'use strict';

async_test(t => {
  let tc = new TaskController("user-visible");
  tc.signal.onprioritychange = t.step_func_done(() => {
    assert_equals(tc.signal.priority, "background");
    assert_throws_dom("NotAllowedError", () => { tc.setPriority("user-blocking"); });
  });
  tc.setPriority("background");
}, 'Test that TaskController.setPriority() throws an error if called recursively');
