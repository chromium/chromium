// This is loaded as a Service Worker in a WPT. When postMessaged to, this
// attempts to forward whatever was sent to the `key-value-store.py` endpoint
// via a fetch() API call. If the fetch() fails, it postMessages back to the WPT
// alerting it to the failure. It also caches a file to test that it can serve
// a cached version of the file after network cutoff.
const STORE_URL = '/wpt_internal/fenced_frame/resources/key-value-store.py';

// This is functionally the same as writeValueToServer() in utils.js.
onmessage = async function(e) {
  const serverUrl = `${STORE_URL}?key=${e.data[0]}&value=${e.data[1]}`;
  try {
    await fetch(serverUrl, {"mode": "no-cors"});
  } catch (error) {
    e.source.postMessage("fetch failed");
  }
}

// Preload resources to be accessed after network cutoff.
const addResourcesToCache = async (resources) => {
  const cache = await caches.open('v1');
  await cache.addAll(resources);
};

self.addEventListener('install', (event) => {
  event.waitUntil(addResourcesToCache([
    '/wpt_internal/fenced_frame/resources/utils.js',
    '/wpt_internal/fenced_frame/resources/attempt-fetch.html',
  ]));
});

// Flip the switch to have network requests route through this worker.
self.addEventListener('activate', function(event) {
  return self.clients.claim();
});

const cacheFirst = async (request) => {
  const responseFromCache = await caches.match(request);
  if (responseFromCache) {
    return responseFromCache;
  }
  return fetch(request);
};

self.addEventListener('fetch', (event) => {
  event.respondWith(cacheFirst(event.request));
});
