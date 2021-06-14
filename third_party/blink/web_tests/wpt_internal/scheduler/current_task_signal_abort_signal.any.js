// META: title=Scheduling API: Signal inheritance
// META: global=window,worker
'use strict';

promise_test(t => {
  let ac = new AbortController()
  return promise_rejects_dom(t, 'AbortError', scheduler.postTask(() => {
    scheduler.postTask(() => {}, { signal : scheduler.currentTaskSignal }).then(
        () => { assert_unreached('This task should have been aborted'); },
        t.step_func_done());
    ac.abort();
  }, { signal: ac.signal }));
}, 'Test that currentTaskSignal wraps and follows an AbortSignal');
