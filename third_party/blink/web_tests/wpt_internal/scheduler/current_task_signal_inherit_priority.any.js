// META: title=Scheduling API: Signal inheritance
// META: global=window
'use strict';

async_test(t => {
  scheduler.postTask(t.step_func_done(() => {
    assert_equals('user-blocking', scheduler.currentTaskSignal.priority);
  }), { priority: "user-blocking" });
}, 'Test that currentTaskSignal propagates priority even if an explicit signal was not given');
