self.addEventListener('fetch', e => {
  const url = e.request.url;

  if (url.includes('prerender/resources/')) {
    // Convert the request headers to an object to send via postMessage().
    const headers = {};
    for (const [key, value] of e.request.headers)
      headers[key] = value;

    // Broadcast the captured request headers.
    const bc = new BroadcastChannel('captured-headers-channel');
    bc.postMessage({ url: e.request.url, headers });
  }

  // Fallback to regular network request.
});
