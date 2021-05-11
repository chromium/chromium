// META: global=window,worker,sharedworker,serviceworker

test(t => {
  assert_false("SharedArrayBuffer" in globalThis);
});
