// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

const cacheName = '{{cache_hash}}';
const filesToCache = [
  'auth.js',
  'auth-consts.js',
  'caspian_web.js',
  'caspian_web.wasm',
  'dom.js',
  'favicon.ico',
  'viewer.html',
  'infocard-ui.js',
  'infocard.css',
  'main.css',
  'manifest.json',
  'metadata-tree-ui.js',
  'metrics-tree-ui.js',
  'options.css',
  'shared.js',
  'start-worker.js',
  'state.js',
  'symbol-tree-ui.js',
  'tree-ui.js',
  'tree-worker-wasm.js',
  'ui-base.js',
  'ui-main.js',
];

// On install, cache the items in the `filesToCache` list
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(cacheName).then(cache => cache.addAll(filesToCache))
  );
});

// On activate, remove any old caches
self.addEventListener('activate', event => {
  async function deleteOldCache(key) {
    if (key !== cacheName) {
      return caches.delete(key);
    }
  }

  event.waitUntil(
    caches.keys().then(keyList => Promise.all(keyList.map(deleteOldCache)))
  );
  return self.clients.claim();
});

// On fetch, return entries from the cache if possible
self.addEventListener('fetch', event => {
  event.respondWith(
    caches
      .match(event.request, {ignoreSearch: true})
      .then(response => response || fetch(event.request))
  );
});
