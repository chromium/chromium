// META: title=Scheduling API: Task Cancellation
// META: global=window
'use strict';

async_test(t => {
  let result = '';
  let task_controllers = [];

  for (let i = 0; i < 5; i++) {
    let tc = new TaskController();
    let task = scheduler.postTask(() => {
      result += i.toString();
    }, { signal: tc.signal });
    if (i == 2) {
      task.then(() => {
        assert_unreached('This task should have been aborted');
      });
    } else {
      task.catch(() => {
        assert_unreached('This task should complete');
      });
    }
    task_controllers.push(tc);
  }

  task_controllers[2].abort();
  scheduler.postTask(t.step_func(() => {
    assert_equals(result, '0134');
  }));

  let final_task_tc = new TaskController();
  // Check that canceling running, completed, or canceled tasks is a no-op.
  scheduler.postTask(t.step_func_done(() => {
    final_task_tc.abort();
    task_controllers[2].abort();
    task_controllers[0].abort();
  }), { signal: final_task_tc.signal });

}, 'Test canceling a task');
