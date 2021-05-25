// META: title=Scheduling API: TaskController.setPriority()
// META: global=window
'use strict';

async_test(t => {
  let tc = new TaskController("user-visible");
  tc.signal.onprioritychange = t.step_func_done((event) => {
    assert_equals(tc.signal.priority, "background");
    assert_equals(event.type, "prioritychange");
    assert_equals(event.target.priority, "background");
    assert_equals(event.previousPriority, "user-visible");
  });
  tc.setPriority("background");
}, 'Test that TaskController.setPriority() triggers an onprioritychange event on the signal');
