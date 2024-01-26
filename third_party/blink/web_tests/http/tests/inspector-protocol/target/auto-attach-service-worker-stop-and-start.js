(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that ServiceWorker starts again after stopped when autoAttach:true and waitForDebuggerOnStart:false`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  let serviceWorkerSessionId;
  let serviceWorkerTargetId;
  dp.Target.onAttachedToTarget(async event => {
    serviceWorkerSessionId = event.params.sessionId;
    serviceWorkerTargetId = event.params.targetInfo.targetId;
    testRunner.log("Attached to Target");
  });
  dp.Target.onDetachedFromTarget(async event => {
    serviceWorkerSessionId = undefined;
    serviceWorkerTargetId = undefined;
    testRunner.log("Detached from Target");
  });

  let version;
  dp.ServiceWorker.onWorkerVersionUpdated(async event => version = event.params.versions.length > 0 ? event.params.versions[0] : undefined);

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();
  await session.navigate("/inspector-protocol/resources/empty.html");

  async function waitForServiceWorkerPhase(phase) {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== phase);
  }
  async function isTargetAttached(targetId) {
    return typeof targetId !== 'undefined' && dp.Target.getTargetInfo({targetId: targetId}).then((result) => result.result.targetInfo.attached).catch(() => false);
  }
  async function getMessageFromServiceWorker() {
    return session.evaluateAsync('new Promise(fulfill => navigator.serviceWorker.onmessage = e => fulfill(e.data))');
  }

  // Register ServiceWorker and check auto attached target exists
  session.evaluateAsync(`navigator.serviceWorker.register('/inspector-protocol/service-worker/resources/push-message-service-worker.js')`);
  await waitForServiceWorkerPhase("activated");
  testRunner.log(`ServiceWorker is activated`);
  testRunner.log(`ServiceWorker RunningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  // Check whether the ServiceWorker is working.
  dp.ServiceWorker.deliverPushMessage({origin: 'http://127.0.0.1:8000', registrationId: version.registrationId, data: 'push message 1'});
  testRunner.log(`Got message: ${await getMessageFromServiceWorker()}`);
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);

  // Stop ServiceWorker and check whether the target is still attached.
  dp.ServiceWorker.stopWorker({versionId: version.versionId});
  await dp.ServiceWorker.onceWorkerVersionUpdated(e => e.params.versions.length && e.params.versions[0].runningStatus === "stopped");
  testRunner.log("ServiceWorker is stopped");
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  // Push message to restart the ServiceWorker.
  dp.ServiceWorker.deliverPushMessage({origin: 'http://127.0.0.1:8000', registrationId: version.registrationId, data: 'push message 2'});
  testRunner.log(`Got message: ${await getMessageFromServiceWorker()}`);
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  // Stop ServiceWorker and check whether the target is still attached.
  dp.ServiceWorker.stopWorker({versionId: version.versionId});
  await dp.ServiceWorker.onceWorkerVersionUpdated(e => e.params.versions.length && e.params.versions[0].runningStatus === "stopped");
  testRunner.log("ServiceWorker is stopped");
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  // Set waitForDebuggerOnStart: true
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  testRunner.log('auto-attach waitForDebuggerOnStart option changed');

  // Push message to restart the ServiceWorker.
  // At this point, ServiceWorker is waiting for debugger.
  dp.ServiceWorker.deliverPushMessage({origin: 'http://127.0.0.1:8000', registrationId: version.registrationId, data: 'push message 3'});
  let message_promise = getMessageFromServiceWorker();
  await dp.ServiceWorker.onceWorkerVersionUpdated(e => e.params.versions.length && e.params.versions[0].runningStatus === "starting");
  // Wait 1 second after the ServiceWorker become 'starting' status
  // to check the ServiceWorker waits for debugger on starting.
  await new Promise(resolve => setTimeout(resolve, 1000));
  testRunner.log("ServiceWorker is waiting for debugger at starting");
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  // Run ServiceWorker and check message
  let dp1 = session.createChild(serviceWorkerSessionId).protocol;
  await dp1.Runtime.runIfWaitingForDebugger();
  await dp.ServiceWorker.onceWorkerVersionUpdated(e => e.params.versions.length && e.params.versions[0].runningStatus === "running");
  testRunner.log("ServiceWorker is started");
  testRunner.log(`Got message: ${await message_promise}`);
  testRunner.log(`ServiceWorker runningStatus: ${version.runningStatus}`);
  testRunner.log(`Target attached: ${await isTargetAttached(serviceWorkerTargetId)}`);

  testRunner.completeTest();
})
