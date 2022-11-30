importScripts("/speculation-rules/prerender/resources/utils.js");

const params = new URLSearchParams(location.search);
const uid = params.get('uid');

self.addEventListener('message', e => {
  // WindowClient::focus() should be called after user activation
  // like notificationclick so we show notification here.
  // See https://w3c.github.io/ServiceWorker/#client-focus
  const title = 'fake notification';
  const promise = registration.showNotification(title).then(_ => {
    e.source.postMessage({type: 'notification was shown', title: title});
  });
  e.waitUntil(promise);
});

self.addEventListener('notificationclick', e => {
  e.notification.close();
  const promise = clients.matchAll()
    .then(clients => {
      // Try to focus on prerendered page.
      const bc = new PrerenderChannel('result-channel', uid);
      const client = clients.find(c => c.url.includes('prerendered-page.html'));
      // The prerendered client should not already be focused.
      if (client.focused) {
        bc.postMessage({ focused: client.focused,
                         result: 'Already focused' });
        bc.close();
        return;
      }
      return client.focus()
        .then(_ => bc.postMessage({ focused: client.focused,
                                    result: 'Focused' }))
        .catch(_ => bc.postMessage({ focused: client.focused,
                                     result: 'Not focused'}))
        .finally(_ => bc.close());
    });
  e.waitUntil(promise);
});
