// META: title=Scheduling API: Task Cancellation
// META: global=window,worker
'use strict';

promise_test(t => {
  let result = '';
  let task_controllers = [];

  for (let i = 0; i < 5; i++) {
    let tc = new TaskController();
    if (i == 2) {
      promise_rejects_dom(t, 'AbortError',
                          scheduler.postTask(() => {
                            result += i.toString();
                          }, { signal: tc.signal }),
                          'This task should have been aborted');
    } else {
      let task = scheduler.postTask(() => {
        result += i.toString();
      }, { signal: tc.signal });

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
  return promise_rejects_dom(t, 'AbortError',
                             scheduler.postTask(t.step_func_done(() => {
                               final_task_tc.abort();
                               task_controllers[2].abort();
                               task_controllers[0].abort();
                             }), { signal: final_task_tc.signal }));
}, 'Test canceling a task');
