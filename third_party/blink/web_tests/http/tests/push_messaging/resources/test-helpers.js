"use strict";

// Subscribes and unsubscribes to push once so that the manifest details are
// stored in service worker storage. After this, subscribe can succeed from
// inside the service worker. This triggers an infobar unless a permission
// decision was previously set.
function subscribeAndUnsubscribePush(registration) {
  return new Promise(function(resolve, reject) {
    // 1. Call subscribe in document context. The manifest details are stored
    // in the service worker storage for later use in a service worker context
    // where there is no manifest.
    registration.pushManager.subscribe({ userVisibleOnly: true }).then(function(subscription) {
      // 2. Call unsubscribe so we can subscribe again later inside a
      // service worker.
      return subscription.unsubscribe();
    })
    .then(function(unsubscription_result) {
      resolve();
    })
    .catch(function(e) {
      reject(e);
    });
  });
}

// Registers a service worker and subscribes to push using the given string
// as an applicationServerKey.
function registerAndSubscribePushWithString(test, serverKeyString) {
  return registerAndSubscribePush(test,
      new TextEncoder().encode(serverKeyString));
}

// Registers a service worker and subscribes to push using the given base64url string
// as an applicationServerKey.
function registerAndSubscribePushWithBase64urlString(test, serverKeyString) {
  return registerAndSubscribePush(test, serverKeyString);
}

// Subscribes to push with the given application server key. serverKey should be
// a Uint8Array.
function registerAndSubscribePush(test, serverKey) {
  const workerUrl = 'resources/empty_worker.js';
  const workerScope = 'resources/scope/' + location.pathname;
  let swRegistration;

  return service_worker_unregister_and_register(test, workerUrl, workerScope)
      .then(function(serviceWorkerRegistration) {
        swRegistration = serviceWorkerRegistration;
        return wait_for_state(test, swRegistration.installing, 'activated');
      })
  .then(function() {
    if (window.testRunner) {
      testRunner.setPermission('push-messaging', 'granted', location.origin,
          location.origin);
    }
    return swRegistration.pushManager.subscribe({
      userVisibleOnly: true,
      applicationServerKey: serverKey
    });
  });
}

// Runs |command| in the service worker connected to |port|. Returns a promise
// that will be resolved with the response data of the command.
function runCommandInServiceWorker(port, command) {
  return new Promise(function(resolve, reject) {
    port.addEventListener('message', function listener(event) {
      // To make this listener a oneshot, remove it the first time it runs.
      port.removeEventListener('message', listener);

      if (typeof event.data != 'object' || !event.data.command)
        assert_unreached('Invalid message from the service worker');

      assert_equals(event.data.command, command);
      if (event.data.success)
        resolve(event.data);
      else
        reject(new Error(event.data.errorMessage));
    });
    port.postMessage({command: command});
  });
}
