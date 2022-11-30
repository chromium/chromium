addEventListener('fetch', event => {
  event.waitUntil((async () => {
    if (!event.clientId) {
      return;
    }

    const client = await clients.get(event.clientId);
    if (!client) {
      return;
    }

    const headers = {};
    for (const pair of event.request.headers.entries()) {
      headers[pair[0]] = pair[1];
    }

    client.postMessage({
      url: event.request.url,
      method: event.request.method,
      referrer: event.request.referrer,
      headers,
    });
  })());
});

addEventListener('activate', event => event.waitUntil(clients.claim()));
addEventListener('install', event => event.waitUntil(skipWaiting()));
