self.addEventListener('fetch', e => {
  if (e.request.url.indexOf("?respond-from-service-worker") !== -1) {
    e.respondWith(fetch(e.request));
  }
});

self.addEventListener('install', event => {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});
