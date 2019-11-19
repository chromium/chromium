self.addEventListener('periodicsync', async event => {
  const clients = await self.clients.matchAll({ includeUncontrolled: true });
  for (const client of clients)
    client.postMessage('test-tag-from-devtools');
});
