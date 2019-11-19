'use strict'

// Depends on /serviceworker/resources/test-helpers.js
async function registerAndActivateServiceWorker(test) {
    const script = '/resources/empty-worker.js';
    const scope = '/resources/scope' + location.pathname;
    const serviceWorkerRegistration =
        await service_worker_unregister_and_register(test, script, scope);
    add_completion_callback(() => serviceWorkerRegistration.unregister());
    await wait_for_state(test, serviceWorkerRegistration.installing, 'activated');
    return serviceWorkerRegistration;
}
