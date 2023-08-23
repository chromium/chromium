// Global state that should be prevented from being garbage collected.
let gController;
let gController2;
let gSignals = [];

function abortSignalAnyMemoryTests(signalInterface, controllerInterface) {
  const suffix = `(using ${signalInterface.name} and ${controllerInterface.name})`;

  // Use promise tests so tests are not interleaved (to prevent global state
  // from getting clobbered).
  promise_test(async t => {
    let wr1;
    let wr2;

    (function() {
      let controller1 = new controllerInterface();
      let controller2 = new controllerInterface();

      gSignals.push(controller1.signal);
      gSignals.push(controller2.signal);

      signal = signalInterface.any(gSignals);
      gSignals.push(signal);

      wr1 = new WeakRef(controller1);
      wr2 = new WeakRef(controller2);
      controller1 = null;
      controller2 = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'controller1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'controller2 should be GCed');
  }, `Controllers can be GCed when their signals are being followed ${suffix}`);

  promise_test(async t => {
    let wr1;
    let wr2;
    let wr3;

    (function() {
      let controller1 = new controllerInterface();
      let controller2 = new controllerInterface();
      let signal = signalInterface.any([controller1.signal, controller2.signal]);

      wr1 = new WeakRef(controller1);
      wr2 = new WeakRef(controller2);
      wr3 = new WeakRef(signal);

      controller1 = null;
      controller2 = null;
      signal = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'controller1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'controller2 should be GCed');
    assert_equals(wr3.deref(), undefined, 'signal should be GCed');
  }, `Signals can be GCed when all abort sources have been GCed ${suffix}`);

  promise_test(async t => {
    let wr1;
    let wr2;

    gController = new controllerInterface();

    (function() {
      let signal1 = signalInterface.any([gController.signal]);
      let signal2 = signalInterface.any([signal1]);

      wr1 = new WeakRef(signal1);
      wr2 = new WeakRef(signal2);

      signal1 = null;
      signal2 = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
  }, `Signals can be GCed when they have no references or event listeners ${suffix}`);

  promise_test(async t => {
    let wr1;
    let wr2;

    gController = new controllerInterface();

    (function() {
      let signal1 = signalInterface.any([gController.signal]);
      signal1.addEventListener('event', () => {});

      let signal2 = signalInterface.any([signal1]);
      signal2.addEventListener('event', () => {});

      wr1 = new WeakRef(signal1);
      wr2 = new WeakRef(signal2);

      signal1 = null;
      signal2 = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
  }, `Signals can be GCed when they have no references or relevant event listeners ${suffix}`);

  promise_test(async t => {
    let wr1;
    let wr2;

    gController = new controllerInterface();

    (function() {
      let signal1 = signalInterface.any([gController.signal]);
      let signal2 = signalInterface.any([signal1]);

      wr1 = new WeakRef(signal1);
      wr2 = new WeakRef(signal2);

      const abortCallback1 = () => {};
      const abortCallback2 = () => {};

      signal1.addEventListener('abort', abortCallback1);
      signal1.addEventListener('abort', abortCallback2);

      signal2.addEventListener('abort', abortCallback1);
      signal2.addEventListener('abort', abortCallback2);

      signal1.removeEventListener('abort', abortCallback1);
      signal1.removeEventListener('abort', abortCallback2);

      signal2.removeEventListener('abort', abortCallback1);
      signal2.removeEventListener('abort', abortCallback2);

      signal1 = null;
      signal2 = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
  }, `Signals can be GCed when all abort event listeners have been removed ${suffix}`);

  promise_test(async t => {
    let fired = false;

    gController = new controllerInterface();

    (function() {
      let signal = signalInterface.any([gController.signal]);
      signal.onabort = t.step_func((e) => {
        fired = true;
        assert_true(e.target.aborted);
      });

      signal = null;
    })();

    await runAsyncGC();

    gController.abort();
    assert_true(fired, 'signal should not be GCed before being aborted');
  }, `Signals are not GCed before being aborted by a controller when they have abort event listeners ${suffix}`);

  promise_test(async t => {
    let fired = false;
    let wr;

    gController = new controllerInterface();

    (function() {
      let signal = signalInterface.any([AbortSignal.timeout(20)]);
      signal.onabort = t.step_func(() => {
        fired = true;
      });
      wr = new WeakRef(signal);
      signal = null;
    })();

    await runAsyncGC();
    await t.step_wait(() => fired, 'The abort listener should run before the signal is GCed', 500, 20);
    await runAsyncGC();
    assert_equals(wr.deref(), undefined, 'signal should be GCed');
  }, `Composite signals are not GCed before being aborted by timeout when they have abort event listeners ${suffix}`);

  promise_test(async t => {
    let fired = false;
    let wr1;
    let wr2;

    gController = new controllerInterface();

    (function() {
      // `tempCompositeSignal` can be GCed after this function because it is
      // only used to construct `compositeSignal`.
      let tempCompositeSignal = signalInterface.any([gController.signal]);
      wr1 = new WeakRef(tempCompositeSignal);

      let compositeSignal = signalInterface.any([tempCompositeSignal]);
      compositeSignal.onabort = t.step_func(() => {
        fired = true;
      });
      wr2 = new WeakRef(compositeSignal);

      tempCompositeSignal = null;
      compositeSignal = null;
    })();

    await runAsyncGC();

    assert_equals(wr1.deref(), undefined, 'tempCompositeSignal should be GCed');
    assert_not_equals(wr2.deref(), undefined, 'compositeSignal shound not be GCed yet');
    assert_false(fired, 'The abort listener should not run before tempCompositeSignal is GCed');

    gController.abort();

    await runAsyncGC();

    assert_equals(wr2.deref(), undefined, 'compositeSignal should be GCed');
    assert_true(fired, 'The abort listener should run before compositeSignal is GCed');
  }, `Temporary composite signals used for constructing other composite signals can be GCed ${suffix}`);

  promise_test(async t => {
    let fired = false;
    let wr1;
    let wr2;
    let wr3;
    let wr4;
    let wr5;

    gController = new controllerInterface();
    gController2 = new controllerInterface();

    (function() {
      // These signals should be GCed after this function runs, before the
      // timeout aborts `testSignal`.
      let signal1 = signalInterface.any([gController.signal]);
      let signal2 = signalInterface.any([gController2.signal]);
      let signal3 = signalInterface.any([signal1, signal2]);

      wr1 = new WeakRef(signal1);
      wr2 = new WeakRef(signal2);
      wr3 = new WeakRef(signal3);

      let timeoutSignal = AbortSignal.timeout(20);
      // This and `timeoutSignal` must remain alive until the timeout fires.
      let testSignal = signalInterface.any([signal3, timeoutSignal]);
      testSignal.onabort = t.step_func(() => {
        fired = true;
      });

      wr4 = new WeakRef(timeoutSignal);
      wr5 = new WeakRef(testSignal);

      signal1 = null;
      signal2 = null;
      signal3 = null;
      timeoutSignal = null;
      testSignal = null;
    })();

    // Running GC async in high priority tasks should complete before the timeout.
    await runAsyncGC({priority: 'user-blocking'});
    assert_false(fired, 'GC should complete before the timeout fires');
    assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
    assert_equals(wr3.deref(), undefined, 'signal3 should be GCed');
    assert_not_equals(wr4.deref(), undefined, 'timeoutSignal should not be GCed before the timeout');
    assert_not_equals(wr5.deref(), undefined, 'testSignal should not be GCed before the timeout');

    await t.step_wait(() => fired, 'The abort listener should run before the signal is GCed', 500, 20);

    await runAsyncGC();
    assert_equals(wr4.deref(), undefined, 'timeoutSignal should be GCed');
    assert_equals(wr5.deref(), undefined, 'testSignal should be GCed');
  }, `Nested and intermediate composite signals can be GCed when expected ${suffix}`);

  promise_test(async t => {
    let wr1;
    let wr2;

    (function() {
      let signal1 = signalInterface.any([]);
      signal1.addEventListener('abort', () => {});
      // For plain AbortSignals, this should not be a no-op. For TaskSignals,
      // this will test the settling logic.
      signal1.addEventListener('prioritychange', () => {});
      wr1 = new WeakRef(signal1);

      let controller = new controllerInterface();
      let signal2 = signalInterface.any([controller.signal]);
      signal2.addEventListener('abort', () => {});
      signal2.addEventListener('prioritychange', () => {});
      wr2 = new WeakRef(signal2);

      signal1 = null;
      signal2 = null;
      controller = null;
    })();

    await runAsyncGC();
    assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
    assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
  }, `Settled composite signals with event listeners can be GCed ${suffix}`);
}
