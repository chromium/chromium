'use strict';

self.addEventListener('install', async e => {
  await e.registerRouter(
    [{ condition: { urlPattern: '*' }, source: 'network' }]);
  self.skipWaiting();
});
self.addEventListener('fetch', e => {
  e.respondWith(new Response('fake handler'));
});
