(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      `Tests that service workers excluded by Target.setAutoAttach filter are NOT paused ` +
      `on start (crbug.com/482620898). Before the fix, the pause decision was made before ` +
      `filters were evaluated, causing a deadlock where the excluded (session-less) worker ` +
      `never received Runtime.runIfWaitingForDebugger.`);

  // Fail fast if the SW deadlocks (bug would cause ~2 min hang).
  testRunner._protocolTimeout = 10_000;

  const swHelper =
      (await testRunner.loadScript(
          '../service-worker/resources/service-worker-helper.js'))(dp, session);

  // Set auto-attach with waitForDebuggerOnStart:true but exclude service_worker via filter.
  // The page-level attachment is used because the service worker script fetch throttle is
  // only applied through page-level TargetHandlers.
  // Filter: [{type: 'service_worker', exclude: true}, {}]
  // This excludes service_worker targets but includes everything else.
  dp.Target.onAttachedToTarget(event => {
    testRunner.log(
        `FAIL: unexpectedly attached to ${event.params.targetInfo.type} target`);
  });
  await dp.Target.setAutoAttach({
    autoAttach: true,
    waitForDebuggerOnStart: true,
    flatten: true,
    filter: [{type: 'service_worker', exclude: true}, {}],
  });

  // Register a service worker and wait for it to become active.
  // If the bug is present, the service worker is paused at startup (via wait_for_debugger)
  // even though it's filtered out. Since no DevTools session is created for filtered-out
  // targets, nobody sends Runtime.runIfWaitingForDebugger, and the worker never starts —
  // causing a deadlock until the worker times out.
  await swHelper.installSWAndWaitForActivated(
      '/inspector-protocol/service-worker/resources/blank-service-worker.js');
  testRunner.log('DONE');
  testRunner.completeTest();
})
