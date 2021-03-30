importScripts('/common/get-host-info.sub.js');

const tmp_url = new URL('simple.js', self.location);
tmp_url.hostname = get_host_info().REMOTE_HOST;
const TARGET_URL = tmp_url.href;

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const cache = await caches.open('padding');
    const response = await fetch(TARGET_URL, { mode: 'no-cors',
                                               cache: 'force-cache' });
    await cache.put(TARGET_URL, response);
    const usage = (await navigator.storage.estimate()).usageDetails.caches;
    await cache.delete(TARGET_URL);

    const client_list = await clients.matchAll({ includeUncontrolled: true });
    for (let client of client_list) {
      client.postMessage({ usage: usage });
    }
  }());
});
