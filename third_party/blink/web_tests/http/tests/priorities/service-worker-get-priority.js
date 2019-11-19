addEventListener('fetch', e => {
  if (e.request.url.endsWith('getPriority'))
    e.respondWith(fetchAndMessagePriority(e.request));
});

async function fetchAndMessagePriority(request) {
  const priorityPromise = internals.getResourcePriority(request.url, self);
  const response = await fetch(request);
  const priority = await priorityPromise;
  const clientArray = await clients.matchAll({includeUncontrolled: true});
  clientArray.forEach(client => {
    client.postMessage(priority);
  });

  return response;
}
