// META: global=window,worker,sharedworker,serviceworker

test(t => {
  assert_true("SharedArrayBuffer" in globalThis);
});
