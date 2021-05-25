// META: title=Scheduling API: Setting TaskController.priority
// META: global=window
'use strict';

async_test(t => {
  let result = '';
  let task_controllers = [];

  for (let i = 0; i < 5; i++) {
    let tc = new TaskController('background');
    scheduler.postTask(() => {
      result += i.toString();
    }, { signal: tc.signal });
    task_controllers.push(tc);
  }

  task_controllers[2].setPriority('user-blocking');
  assert_equals(task_controllers[2].signal.priority, 'user-blocking');

  scheduler.postTask(t.step_func_done(() => {
    assert_equals('20134', result);
  }), { priority: 'background' });

}, 'Test modifying TaskController priority');
