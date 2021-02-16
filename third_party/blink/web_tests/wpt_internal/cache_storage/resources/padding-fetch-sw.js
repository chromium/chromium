self.addEventListener('fetch', evt => {
  if (!evt.request.url.includes('simple.js') || evt.request.mode != 'no-cors')
    return;

  evt.respondWith(async function() {
    // Fetch the response from the network.
    const net_response = await fetch(evt.request);

    // Before returning the response store a clone in cache_storage
    // and compute its padded usage.
    const cache = await caches.open('padding');
    // Make sure to use the URL and not the full request.  The main window
    // and service worker requests may have different headers which can
    // throw off the size comparison.
    await cache.put(evt.request.url, net_response.clone());
    const usage = (await navigator.storage.estimate()).usageDetails.caches;
    await cache.delete(evt.request.url);

    // Send the padded usage value to the main window so that it can
    // be compared.
    const client = await clients.get(evt.clientId);
    client.postMessage({ usage: usage });

    // Finally return the original network response.
    return net_response;
  }());
});
