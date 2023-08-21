// Global state that should be prevented from being garbage collected.
let gRegistry;
let gController;
let gSignals = [];

// The tests below rely on the same global state, which each test manipulates.
// Use promise_tests so tests are not interleaved, otherwise the global state
// can change unexpectedly.
promise_test(t => {
  return new Promise((resolve) => {
    let tokens = [];
    gController = new TaskController();
    gRegistry = new FinalizationRegistry(t.step_func(function(token) {
      tokens.push(token);
      if (tokens.length == 3) {
        assert_in_array(1, tokens);
        assert_in_array(2, tokens);
        assert_in_array(3, tokens);
        resolve();
      }
    }));

    (function() {
      let signal1 = TaskSignal.any([], {priority: gController.signal});
      let signal2 = TaskSignal.any([gController.signal], {priority: gController.signal});
      let signal3 = TaskSignal.any([signal2]);

      gRegistry.register(signal1, 1);
      gRegistry.register(signal2, 2);
      gRegistry.register(signal3, 3);

      signal1 = null;
      signal2 = null;
      signal3 = null;
    })();

    gc();
  });
}, "TaskSignals can be GCed when they have no references or event listeners");

promise_test(t => {
  return new Promise((resolve) => {
    let tokens = [];
    gRegistry = new FinalizationRegistry(t.step_func(function(token) {
      tokens.push(token);
      if (tokens.length == 2) {
        assert_in_array(1, tokens);
        assert_in_array(2, tokens);
        resolve();
      }
    }));

    (function() {
      let controller = new TaskController();
      let signal = TaskSignal.any([], {priority: controller.signal});
      signal.onprioritychange = () => {};

      gRegistry.register(controller, 1);
      gRegistry.register(signal, 2);

      controller = null;
      signal = null;
    })();

    gc();
  });
}, "A TaskSignal with a prioritychange listener can be GCed when its priority source has been GCed");

promise_test(t => {
  return new Promise((resolve) => {
    (function() {
      gController = new TaskController();
      let signal = TaskSignal.any([], {priority: gController.signal});
      signal.onprioritychange = t.step_func((e) => {
        assert_equals(e.target.priority, 'background');
        resolve();
      });
      signal = null;
    })();

    gc();
    gController.setPriority('background');
  });
}, "TaskSignals with prioritychange listeners are not GCed if their priority source is alive");

promise_test(t => {
  return new Promise((resolve) => {
    (function() {
      gController = new TaskController();
      let controller = new AbortController();
      let signal = TaskSignal.any([controller.signal], {priority: gController.signal});
      signal.onprioritychange = t.step_func((e) => {
        assert_equals(e.target.priority, 'background');
        resolve();
      });
      signal = null;
      controller = null;
    })();

    gc();
    gController.setPriority('background');
  });
}, "TaskSignals with prioritychange listeners are not GCed after their abort source is GCed if their priority source is alive");

promise_test(t => {
  return new Promise((resolve) => {
    (function() {
      gController = new TaskController();
      let controller = new AbortController();
      let signal = TaskSignal.any([controller.signal], {priority: gController.signal});
      signal.onprioritychange = t.step_func((e) => {
        assert_equals(e.target.priority, 'background');
        resolve();
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

    gc();
    gController.setPriority('background');
  });
}, "TaskSignals with prioritychange listeners are not GCed after they are aborted if their priority source is alive");

promise_test(t => {
  return new Promise((resolve) => {
    let runCount = 0;
    gRegistry = new FinalizationRegistry(t.step_func(function(token) {
      assert_equals(token, 1);
      assert_equals(runCount, 3);
      resolve();
    }));
    gController = new TaskController({priority: 'background'});

    (function() {
      let signal = TaskSignal.any([], {priority: gController.signal});
      scheduler.postTask(() => { ++runCount; }, {signal});
      scheduler.postTask(() => { ++runCount; }, {signal});
      scheduler.postTask(() => { ++runCount; }, {signal});

      // Finally, gc in a separate task so `signal` can be GCed.
      scheduler.postTask(() => { gc(); }, {priority: 'background'});

      gRegistry.register(signal, 1);
      signal = null;
    })();

    gc();
  });
}, "Composite TaskSignals with pending tasks are not GCed if their priority source is alive");
