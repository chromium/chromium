// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Service Worker initialization listeners.
self.addEventListener('install', event => event.waitUntil(skipWaiting()));
self.addEventListener('activate', event => event.waitUntil(clients.claim()));

self.addEventListener('sync', event => {
  event.waitUntil(fetch(
    '/background_sync_browsertest.html?syncreceived'));
});
