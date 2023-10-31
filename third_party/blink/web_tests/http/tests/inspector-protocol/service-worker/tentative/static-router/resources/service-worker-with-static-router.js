'use strict';

self.addEventListener('install', async e => {
  await e.registerRouter([
    {condition: {requestMethod: 'POST'}, source: 'network'},
    {condition: {urlPattern: '/**/*.txt??*'}, source: 'fetch-event'}
  ]);
  self.skipWaiting();
});
