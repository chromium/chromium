const cacheName = 'c8-cache-test';
const scriptName = '/devtools/resources/v8-cache-script.js';

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(cacheName).then(
      (cache) => cache.add(new Request(scriptName))));
});

self.addEventListener('fetch', (event) => {
  if (event.request.url.indexOf(scriptName) == -1) {
    return;
  }
  event.respondWith(caches.open(cacheName).then((cache) => {
    return cache.match(new Request(scriptName));
  }));
});
