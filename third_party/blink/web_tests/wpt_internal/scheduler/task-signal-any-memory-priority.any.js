// META: script=../dom/abort/resources/run-async-gc.js

// Global state that should be prevented from being garbage collected.
let gController;
let gSignals = [];

// The tests below rely on the same global state, which each test manipulates.
// Use promise_tests so tests are not interleaved, otherwise the global state
// can change unexpectedly.
promise_test(async t => {
  let wr1;
  let wr2;
  let wr3;

  gController = new TaskController();

  (function() {
    let signal1 = TaskSignal.any([], {priority: gController.signal});
    let signal2 = TaskSignal.any([gController.signal], {priority: gController.signal});
    let signal3 = TaskSignal.any([signal2]);

     wr1 = new WeakRef(signal1);
     wr2 = new WeakRef(signal2);
     wr3 = new WeakRef(signal3);

    signal1 = null;
    signal2 = null;
    signal3 = null;
  })();

  await runAsyncGC();

  assert_equals(wr1.deref(), undefined, 'signal1 should be GCed');
  assert_equals(wr2.deref(), undefined, 'signal2 should be GCed');
  assert_equals(wr3.deref(), undefined, 'signal3 should be GCed');
}, "TaskSignals can be GCed when they have no references or event listeners");

promise_test(async t => {
  let wr1;
  let wr2;

  (function() {
    let controller = new TaskController();
    let signal = TaskSignal.any([], {priority: controller.signal});
    signal.onprioritychange = () => {};

    wr1 = new WeakRef(controller);
    wr2 = new WeakRef(signal);

    controller = null;
    signal = null;
  })();

  await runAsyncGC();
  assert_equals(wr1.deref(), undefined, 'controller should be GCed');
  assert_equals(wr2.deref(), undefined, 'signal should be GCed');
}, "A TaskSignal with a prioritychange listener can be GCed when its priority source has been GCed");

promise_test(async t => {
  let fired = false;

  (function() {
    gController = new TaskController();
    let signal = TaskSignal.any([], {priority: gController.signal});
    signal.onprioritychange = t.step_func((e) => {
      assert_equals(e.target.priority, 'background', 'Priority should change to background');
      fired = true;
    });
    signal = null;
  })();

  await runAsyncGC();
  gController.setPriority('background');
  assert_true(fired, 'prioritchange event should fire');
}, "TaskSignals with prioritychange listeners are not GCed if their priority source is alive");

promise_test(async t => {
  (function() {
    gController = new TaskController();
    let controller = new AbortController();
    let signal = TaskSignal.any([controller.signal], {priority: gController.signal});
    signal.onprioritychange = t.step_func((e) => {
      assert_equals(e.target.priority, 'background', 'Priority should change to background');
      fired = true;
    });
    signal = null;
    controller = null;
  })();

  await runAsyncGC();
  gController.setPriority('background');
  assert_true(fired, 'prioritchange event should fire');
}, "TaskSignals with prioritychange listeners are not GCed after their abort source is GCed if their priority source is alive");

promise_test(async t => {
  let fired = true;

  (function() {
    gController = new TaskController();
    let controller = new AbortController();
    let signal = TaskSignal.any([controller.signal], {priority: gController.signal});
    signal.onprioritychange = t.step_func((e) => {
      assert_equals(e.target.priority, 'background');
      fired = true;
    });

    let abortFired = false;
    signal.onabort = t.step_func(() => {
      abortFired = true;
    });
    controller.abort();
    assert_true(abortFired);

    signal = null;
    controller = null;
  })();

  await runAsyncGC();
  gController.setPriority('background');
  assert_true(fired, 'prioritchange event should fire');
}, "TaskSignals with prioritychange listeners are not GCed after they are aborted if their priority source is alive");

promise_test(async t => {
  let runCount = 0;
  gController = new TaskController({priority: 'background'});
  const tasks = [];

  (function() {
    let signal = TaskSignal.any([], {priority: gController.signal});
    scheduler.postTask(() => { ++runCount; }, {signal});
    scheduler.postTask(() => { ++runCount; }, {signal});
    scheduler.postTask(() => { ++runCount; }, {signal});

    wr = new WeakRef(signal);
    signal = null;
  })();

  // Since this runs at higher than background priority, nothing should have
  // happened yet.
  await runAsyncGC();
  assert_not_equals(wr.deref(), undefined, 'signal should not have been GCed yet');

  // Let the background tasks run.
  // NB: we don't use the task promises since the signal will be propagated for
  // yield inheritance.
  await scheduler.postTask(() => {}, {priority: 'background'});
  assert_equals(runCount, 3, '3 tasks should have run');

  // Finally, run gc so `signal` can be GCed.
  await runAsyncGC();
  assert_equals(wr.deref(), undefined, 'signal should have been GCed');
}, "Composite TaskSignals with pending tasks are not GCed if their priority source is alive");
