// META: title=Scheduling API: Tasks Run in Priority Order
// META: global=window

async_test(t => {
  let result = '';

  function report(id) {
    result = result === '' ? id : result + ' ' + id;
  }

  function scheduleReportTask(id, priority) {
    scheduler.postTask(() => {
      report(id);
    }, { priority });
  }

  // Post tasks in reverse priority order and expect they are run from highest
  // to lowest priority.
  scheduleReportTask('B1', 'background');
  scheduleReportTask('B2', 'background');
  scheduleReportTask('UV1', 'user-visible');
  scheduleReportTask('UV2', 'user-visible');
  scheduleReportTask('UB1', 'user-blocking');
  scheduleReportTask('UB2', 'user-blocking');

  scheduler.postTask(t.step_func_done(() => {
    assert_equals(result, 'UB1 UB2 UV1 UV2 B1 B2');
  }), { priority: 'background' });
}, 'Test scheduler.postTask task run in priority order');
