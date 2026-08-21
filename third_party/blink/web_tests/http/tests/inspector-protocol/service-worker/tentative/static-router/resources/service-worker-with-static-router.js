'use strict';

self.addEventListener('install', async e => {
  // TODO(crbug.com/548227769): Add test coverage for
  // race-network-and-fetch-handler, cache, and named cache sources.
  await e.addRoutes([
    {condition: {requestMethod: 'POST'}, source: 'network'},
    {condition: {urlPattern: '/**/*.txt??*'}, source: 'fetch-event'}
  ]);
  self.skipWaiting();
});

// If the "fetch-event" source is set, it is mandatory to have a fetch
// handler.
// See: https://github.com/yoshisatoyanagisawa/ServiceWorker/pull/2.
self.addEventListener('fetch', {});
