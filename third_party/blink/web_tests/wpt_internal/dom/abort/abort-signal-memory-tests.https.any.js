// META: script=./resources/abort-signal-any-memory-tests.js

abortSignalAnyMemoryTests(AbortSignal, AbortController);

test(t => {
  let count = 0;
  const controller = new AbortController();
  const signal = controller.signal;
  addEventListener('test', () => { ++count; }, {signal});

  // GC should not affect the event dispatch or listener removal below.
  gc();

  dispatchEvent(new Event('test'));
  dispatchEvent(new Event('test'));

  assert_equals(count, 2);

  controller.abort();
  dispatchEvent(new Event('test'));
  assert_equals(count, 2);
}, 'AbortSignalRegistry tracks algorithm handles for event listeners');

test(t => {
  let count = 0;
  const controller = new AbortController();

  (function() {
    let signal = AbortSignal.any([controller.signal]);
    addEventListener('test2', () => { ++count; }, {signal});
    signal = null;
  })();

  dispatchEvent(new Event('test2'));
  dispatchEvent(new Event('test2'));

  assert_equals(count, 2);

  // GC should not affect the listener removal below. The composite signal
  // above is not held onto by JS, so this test will fail if nothing is
  // holding a reference to it.
  gc();

  controller.abort();
  dispatchEvent(new Event('test2'));
  assert_equals(count, 2);
}, 'AbortSignalRegistry tracks algorithm handles for event listeners (composite signal)');

promise_test(t => {
  const controller = new AbortController();
  let promise;

  (function() {
    let signal = AbortSignal.any([controller.signal]);
    promise = navigator.locks.request("abort-signal-any-test-lock", {signal}, lock => {});
    signal = null;
  })();

  // Make sure the composite signal isn't GCed even though the lock request
  // doesn't hold onto it.
  gc();

  controller.abort();
  return promise_rejects_dom(t, 'AbortError', promise);
}, 'Lock request keeps composite signal alive');
