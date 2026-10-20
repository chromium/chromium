self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  event.respondWith(event.request.url.endsWith('/synthesized') ?
                        new Response('Synthesized by service worker') :
                        fetch(event.request));
});
