// META: title=Scheduling API: Task.result When a Task Throws an Error
// META: global=window
'use strict';

async_test(t => {
  (function() {
    let task_promise = scheduler.postTask(() => { throw Error('Failed'); });
    task_promise.then(t.step_func((res) => {
      assert_true(false, 'task promise should not be fulfilled when the task throws an error.');
    }))
    .catch(t.step_func_done((e) => {
      assert_equals(e.name, 'Error');
      assert_equals(e.message, 'Failed');
    }));
  })();
}, 'Test task promise is rejected properly when the task throws an error');
