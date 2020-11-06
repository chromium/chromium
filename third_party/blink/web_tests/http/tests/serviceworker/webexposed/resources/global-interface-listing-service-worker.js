function unregister_service_worker(document_url) {
  return navigator.serviceWorker.getRegistration(document_url)
    .then(registration => {
        if (registration)
          return registration.unregister();
    });
}

function register_service_worker(platformSpecific) {
  var scope = 'resources/global-interface-listing-worker';
  var options = { scope: scope };
  unregister_service_worker(scope)
    .then(() => {
        return navigator.serviceWorker.register(
            'resources/global-interface-listing-worker.js',
            options);
      })
    .then(registration => {
        var sw = registration.installing;
        return new Promise(resolve => {
            var message_channel = new MessageChannel;
            message_channel.port1.onmessage = (evt => resolve(evt));
            sw.postMessage({ platformSpecific }, [message_channel.port2]);
          });
      })
    .then(evt => {
        var pre = document.createElement('pre');
        pre.appendChild(document.createTextNode(evt.data.result.join('\n')));
        document.body.insertBefore(pre, document.body.firstChild);
        return unregister_service_worker(scope);
      })
    .then(() => {
        testPassed('Verify the interface of ServiceWorkerGlobalScope');
        finishJSTest();
      })
    .catch(error =>  {
        testFailed('error: ' + (error.message || error.name || error));
        finishJSTest();
      });
}