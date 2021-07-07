self.onmessage = e => {
  const navigationUrl = e.data.navigationUrl;
  const clientUrl = e.data.clientUrl;
  const respondTo = e.data.respondTo;

  e.waitUntil(async function() {
    const source = e.source;
    const clients = await self.clients.matchAll();
    const client = clients.find(c => c.url == clientUrl);
    if (!client) {
      const bc = new BroadcastChannel(respondTo);
      bc.postMessage('Client was not found');
      bc.close();
      return;
    }

    let result;
    try {
      await client.navigate(navigationUrl);
      result = 'navigate() succeeded';
    } catch (e) {
      if (e instanceof TypeError) {
        result = 'navigate() failed with TypeError';
      } else {
        result = 'navigate() failed with unknown error';
      }
    } finally {
      const bc = new BroadcastChannel(respondTo);
      bc.postMessage(result);
      bc.close();
    }
  }());
};
