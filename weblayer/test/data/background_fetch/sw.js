// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service Worker initialization listeners.
self.addEventListener('install', e => e.waitUntil(skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(clients.claim()));

// Posts |msg| to background_fetch.js.
function postToWindowClients(msg) {
  return clients.matchAll({ type: 'window' }).then(clientWindows => {
    for (const client of clientWindows) client.postMessage(msg);
  });
}

self.addEventListener('backgroundfetchsuccess', e => {
  console.log('background fetch succeeded');
  e.waitUntil(e.updateUI({title: 'New Fetched Title!'}));
});

self.addEventListener('backgroundfetchclick', e => {
  e.waitUntil(clients.openWindow(
      '/weblayer/test/data/service_worker/new_window.html'));
});
