self.onmessage = e => {
  const url = e.data;

  e.waitUntil(async function() {
    const source = e.source;
    const clients = await self.clients.matchAll();
    const client = clients.find(c => c.url.includes('prerendering'));
    if (!client) {
      source.postMessage('Client was not found');
      return;
    }

    try {
      await client.navigate(url);
      source.postMessage('navigate() succeeded');
      return;
    } catch (e) {
      if (e instanceof TypeError) {
        source.postMessage('navigate() failed with TypeError');
      } else {
        source.postMessage('navigate() failed with unknown error');
      }
      return;
    }
  }());
};
