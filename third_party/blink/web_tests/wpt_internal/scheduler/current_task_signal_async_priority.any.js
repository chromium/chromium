// META: title=Scheduling API: Signal inheritance
// META: global=window
'use strict';

async_test(t => {
  let tc = new TaskController("user-blocking");
  scheduler.postTask(async () => {
    await new Promise(resolve => setTimeout(resolve, 0));
    assert_equals(scheduler.currentTaskSignal.priority, "user-blocking");
    t.done();
  }, { signal: tc.signal });
}, 'Test that currentTaskSignal works through promise resolution');
