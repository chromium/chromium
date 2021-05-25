// META: title=Scheduling API: Signal inheritance
// META: global=window
'use strict';

async_test(t => {
  let tc = new TaskController("user-blocking");
  scheduler.postTask(async () => {
    await fetch("support/dummy.txt");
    scheduler.postTask(() => {}, { signal: scheduler.currentTaskSignal }).then(
      () => { assert_unreached('This task should have been aborted'); },
      t.step_func_done());
    tc.abort();
  }, { signal: tc.signal });
}, 'Test that currentTaskSignal works through promise resolution');
