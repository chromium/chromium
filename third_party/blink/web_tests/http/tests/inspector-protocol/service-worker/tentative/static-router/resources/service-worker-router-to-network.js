'use strict';

self.addEventListener('install', async e => {
  await e.registerRouter(
      [{condition: {urlPattern: new URLPattern({})}, source: 'network'}]);
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  e.respondWith(new Response('fake handler'));
});
