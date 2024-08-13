promise_test(async t => {
  let wr1;
  let wr2;

  (function () {
    const { port1, port2 } = new MessageChannel();
    port2.start();
    wr1 = new WeakRef(port1);
    wr2 = new WeakRef(port2);
  })()

  await gc({type: 'major', execution: 'async'});
  assert_equals(wr1.deref(), undefined, 'port1 should be GCed');

  // `port2` won't be eligible for GC until the connection is closed, which
  // happens asynchronously after `port1` is GCed.
  await gc({type: 'major', execution: 'async'});
  assert_equals(wr2.deref(), undefined, 'port2 should be GCed');
}, 'Message ports get GCed after they are no longer referenced.');
