// Global state that should be prevented from being garbage collected.
let gRegistry;
let gController;
let gController2;
let gSignals = [];

function abortSignalAnyMemoryTests(signalInterface, controllerInterface) {
  const suffix = `(using ${signalInterface.name} and ${controllerInterface.name})`;

  // Schedules a GC to run before any pending finalization registry callbacks.
  // This depends on user-blocking tasks running at a higher priority than
  // main-thread V8 tasks.
  const scheduleHighPriorityGC = () => scheduler.postTask(() => { gc(); }, {priority: 'user-blocking'});

  // Use promise tests so tests are not interleaved (to prevent global state
  // from getting clobbered).
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
        let controller1 = new controllerInterface();
        let controller2 = new controllerInterface();

        gSignals.push(controller1.signal);
        gSignals.push(controller2.signal);

        signal = signalInterface.any(gSignals);
        gSignals.push(signal);

        gRegistry.register(controller1, 1);
        gRegistry.register(controller2, 2);
        controller1 = null;
        controller2 = null;
      })();

      gc();
    });
  }, `Controllers can be GCed when their signals are being followed ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokens = [];
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
        let controller1 = new controllerInterface();
        let controller2 = new controllerInterface();
        let signal = signalInterface.any([controller1.signal, controller2.signal]);

        gRegistry.register(controller1, 1);
        gRegistry.register(controller2, 2);
        gRegistry.register(signal, 3);

        controller1 = null;
        controller2 = null;
        signal = null;
      })();

      gc();
    });
  }, `Signals can be GCed when all abort sources have been GCed ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokens = [];
      gController = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        tokens.push(token);
        if (tokens.length == 2) {
          assert_false(gController.signal.aborted);
          assert_in_array(1, tokens);
          assert_in_array(2, tokens);
          resolve();
        }
      }));

      (function() {
        let signal1 = signalInterface.any([gController.signal]);
        let signal2 = signalInterface.any([signal1]);

        gRegistry.register(signal1, 1);
        gRegistry.register(signal2, 2);

        signal1 = null;
        signal2 = null;
      })();

      gc();
    });
  }, `Signals can be GCed when they have no references or event listeners ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokens = [];
      gController = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        tokens.push(token);
        if (tokens.length == 2) {
          assert_false(gController.signal.aborted);
          assert_in_array(1, tokens);
          assert_in_array(2, tokens);
          resolve();
        }
      }));

      (function() {
        let signal1 = signalInterface.any([gController.signal]);
        signal1.addEventListener('event', () => {});

        let signal2 = signalInterface.any([signal1]);
        signal2.addEventListener('event', () => {});

        gRegistry.register(signal1, 1);
        gRegistry.register(signal2, 2);

        signal1 = null;
        signal2 = null;
      })();

      gc();
    });
  }, `Signals can be GCed when they have no references or relevant event listeners ${suffix}`);

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

      gController = new controllerInterface();

      (function() {
        let signal1 = signalInterface.any([gController.signal]);
        let signal2 = signalInterface.any([signal1]);

        gRegistry.register(signal1, 1);
        gRegistry.register(signal2, 2);

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

      gc();
    });
  }, `Signals can be GCed when all abort event listeners have been removed ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokenCount = 0;
      let fired = false;

      gController = new controllerInterface();

      (function() {
        let signal = signalInterface.any([gController.signal]);
        signal.onabort = t.step_func((e) => {
          assert_true(e.target.aborted);
          resolve();
        });

        signal = null;
      })();

      gc();
      gController.abort();
    });
  }, `Signals are not GCed before being aborted by a controller when they have abort event listeners ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      gController = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        assert_equals(token, 1);
        assert_true(fired, 'The abort listener should not run before the signal is GCed');
        resolve();
      }));

      (function() {
        let signal = signalInterface.any([AbortSignal.timeout(20)]);
        signal.onabort = t.step_func(() => {
          fired = true;
          // GC could also be triggered in this timeout task, so run GC at high
          // priority to ensure the task finishes before the test.
          scheduleHighPriorityGC();
        });
        gRegistry.register(signal, 1);
        signal = null;
      })();

      gc();
    });
  }, `Composite signals are not GCed before being aborted by timeout when they have abort event listeners ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokenCount = 0;
      let fired = false;

      gController = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        ++tokenCount;
        if (tokenCount == 1) {
          assert_equals(token, 1, 'tempCompositeSignal should be GCed first');
          assert_false(fired, 'The abort listener should not run before tempCompositeSignal is GCed');
          gController.abort();
          gc();
        }

        if (tokenCount == 2) {
          assert_equals(token, 2, 'compositeSignal should be GCed second');
          assert_true(fired, 'The abort listener should run before compositeSignal is GCed');
          resolve();
        }
      }));

      (function() {
        // `tempCompositeSignal` can be GCed after this function because it is
        // only used to construct `compositeSignal`.
        let tempCompositeSignal = signalInterface.any([gController.signal]);
        gRegistry.register(tempCompositeSignal, 1);

        let compositeSignal = signalInterface.any([tempCompositeSignal]);
        compositeSignal.onabort = t.step_func(() => {
          fired = true;
        });
        gRegistry.register(compositeSignal, 2);

        tempCompositeSignal = null;
        compositeSignal = null;
      })();

      gc();
    });
  }, `Temporary composite signals used for constructing other composite signals can be GCed ${suffix}`);

  promise_test(t => {
    return new Promise((resolve) => {
      let tokenCount = 0;
      let fired = false;
      let tokens = [];

      gController = new controllerInterface();
      gController2 = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        ++tokenCount;
        tokens.push(token);

        if (tokenCount == 3) {
          assert_array_equals(tokens.sort(), [1, 2, 3], 'The temporary signals should be GCed first');
          assert_false(fired, 'The temporary signals should be GCed before the abort event listener fires');
        }

        if (tokenCount == 5) {
          assert_true(fired, 'The abort listener should run before compositeSignal is GCed');
          resolve();
        }
      }));

      (function() {
        // These signals should be GCed after this function runs, before the
        // timeout aborts `testSignal`.
        let signal1 = signalInterface.any([gController.signal]);
        let signal2 = signalInterface.any([gController2.signal]);
        let signal3 = signalInterface.any([signal1, signal2]);

        let timeoutSignal = AbortSignal.timeout(20);
        // This and `timeoutSignal` must remain alive until the timeout fires.
        let testSignal = signalInterface.any([signal3, timeoutSignal]);
        testSignal.onabort = t.step_func(() => {
          fired = true;
          scheduleHighPriorityGC();
        });

        gRegistry.register(signal1, 1);
        gRegistry.register(signal2, 2);
        gRegistry.register(signal3, 3);
        gRegistry.register(timeoutSignal, 4);
        gRegistry.register(testSignal, 5);

        signal1 = null;
        signal2 = null;
        signal3 = null;
        timeoutSignal = null;
        testSignal = null;
      })();

      gc();
    });
  }, `Nested and intermediate composite signals can be GCed when expected ${suffix}`);
}
