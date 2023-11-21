'use strict';

self.addEventListener('install', async e => {
  await e.registerRouter(
      [{condition: {urlPattern: '*'}, source: 'fetch-event'}]);
  self.skipWaiting();
});

self.addEventListener('fetch', e => {
  e.respondWith(new Response('fetch event'));
});
