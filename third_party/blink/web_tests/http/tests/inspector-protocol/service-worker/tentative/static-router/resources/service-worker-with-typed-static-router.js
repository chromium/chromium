'use strict';

self.addEventListener('install', async e => {
  // TODO(crbug.com/548227769): Add test coverage for
  // race-network-and-fetch-handler, cache, and named cache sources.
  await e.addRoutes([
    {condition: {requestMethod: 'POST'}, source: 'network'}, {
      condition: {urlPattern: 'https://example.com/test/*'},
      source: 'fetch-event'
    },
    {condition: {urlPattern: 'https://example.com'}, source: 'fetch-event'}, {
      condition: {urlPattern: 'https://example.com/test?query=test#test_hash'},
      source: 'fetch-event'
    },
    {
      condition: {urlPattern: 'https://example.com/test/*\\?query=test'},
      source: 'fetch-event'
    },
    {
      condition: {urlPattern: 'https://user@example.com/test/*'},
      source: 'fetch-event'
    },
    {condition: {urlPattern: '/**/*.txt??*'}, source: 'fetch-event'},
    {condition: {urlPattern: new URLPattern({})}, source: 'fetch-event'},
    {condition: {or: []}, source: 'network'}, {
      condition: {
        or: [
          {not: {urlPattern: 'https://example.com/test/*'}},
          {requestMethod: 'POST'}
        ]
      },
      source: 'network'
    },
    {condition: {not: {requestMethod: 'GET'}}, source: 'network'}
  ]);
  self.skipWaiting();
});

// If the "fetch-event" source is set, it is mandatory to have a fetch
// handler.
// See: https://github.com/yoshisatoyanagisawa/ServiceWorker/pull/2.
self.addEventListener('fetch', {});
