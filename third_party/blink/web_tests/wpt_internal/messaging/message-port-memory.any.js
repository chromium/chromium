// META: script=/wpt_internal/dom/abort/resources/run-async-gc.js

// Use promise tests so tests are not interleaved (to prevent global state
// from getting clobbered).
promise_test(async t => {
  let wr1;
  let wr2;

  (function () {
    const { port1, port2 } = new MessageChannel();
    port2.start();
    wr1 = new WeakRef(port1);
    wr2 = new WeakRef(port2);
  })()

  await runAsyncGC();

  assert_equals(wr1.deref(), undefined, 'port1 should be GCed');
  assert_equals(wr2.deref(), undefined, 'port2 should be GCed');
}, 'Message ports get GCed after they are no longer referenced.');

