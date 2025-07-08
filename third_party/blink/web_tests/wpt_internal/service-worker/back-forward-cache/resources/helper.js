// Triggers the service worker to send a message using a transferred port.
async function postMessageViaTransferredPort(t, worker) {
  const channel = new MessageChannel();
  const sawMessage = new Promise(resolve => {
    channel.port1.onmessage = t.step_func(e => {
      assert_equals(e.data, 'PASS', 'SW should confirm message was sent.');
      resolve();
    });
  });
  worker.postMessage(
      {type: 'postMessageViaTransferredPort', port: channel.port2},
      [channel.port2]);
  await sawMessage;
}

// Sets up a service worker and creates a new page controlled by the worker.
async function createServiceWorkerControlledPage(
    t,
    workerUrl =
        '/wpt_internal/service-worker/back-forward-cache/resources/service-worker.js?pipe=header(Service-Worker-Allowed,/)',
    scope = '/') {
  const registration =
      await service_worker_unregister_and_register(t, workerUrl, scope);
  t.add_cleanup(() => registration.unregister());
  await wait_for_state(t, registration.installing, 'activated');
  const controllerChanged = new Promise(
      resolve => navigator.serviceWorker.oncontrollerchange = resolve);
  await claim(t, registration.active);
  await controllerChanged;

  const rcHelper = new RemoteContextHelper();
  const page = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  assert_true(
      await page.executeScript(
          () => (navigator.serviceWorker.controller !== null)),
      'Page should be controlled before navigation');

  return {registration, page};
}
