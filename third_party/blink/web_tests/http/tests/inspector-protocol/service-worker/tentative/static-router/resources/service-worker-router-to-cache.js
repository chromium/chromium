'use strict';

const TEST_CACHE_NAME = 'v1';
self.addEventListener('install', async e => {
  e.waitUntil(caches.open(TEST_CACHE_NAME).then(
      cache => {cache.put('cache.txt', new Response('From cache'))}));

  await e.addRoutes(
      [{condition: {urlPattern: new URLPattern({})}, source: 'cache'}]);
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  e.respondWith(new Response('fake handler'));
});
