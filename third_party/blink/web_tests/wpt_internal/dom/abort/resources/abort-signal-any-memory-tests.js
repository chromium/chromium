// Global state that should be prevented from being garbage collected.
let gRegistry;
let gController;
let gSignals = [];

function abortSignalAnyMemoryTests(signalInterface, controllerInterface) {
  const suffix = `(using ${signalInterface.name} and ${controllerInterface.name})`;

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
      let tokenCount = 0;
      let fired = false;

      gController = new controllerInterface();
      gRegistry = new FinalizationRegistry(t.step_func(function(token) {
        ++tokenCount;
        if (tokenCount == 1) {
          assert_equals(token, 1, 'signal1 should be GCed first');
          assert_false(fired, 'The abort listener should not run before signal1 is GCed');
          gController.abort();
          gc();
        }

        if (tokenCount == 2) {
          assert_equals(token, 2, 'signal2 should be GCed second');
          assert_true(fired, 'The abort listener should run before signal2 is GCed');
          t.done();
        }
      }));

      (function() {
        let signal1 = signalInterface.any([gController.signal]);
        gRegistry.register(signal1, 1);

        let signal2 = signalInterface.any([signal1, signalInterface.timeout(500)]);
        signal2.onabort = t.step_func(() => {
          fired = true;
        });
        gRegistry.register(signal2, 2);

        signal1 = null;
        signal2 = null;
      })();

      gc();
    });
  }, `Signals are not GCed before being aborted by timeout when they have abort event listeners ${suffix}`);
}
