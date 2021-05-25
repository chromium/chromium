// META: title=Scheduling API: Task.result Accessed Before Task Runs
// META: global=window
'use strict';

async_test(t => {
  (function() {
    scheduler.postTask(() => 1234).then(t.step_func_done((res) => {
      assert_equals(res, 1234);
    }));
  })();
}, 'Test task result is resolved properly when accessed before the task runs');
