var eventHandler = async function (event) {
  event.respondWith(new Response('codeSupposedUnreachable'));
};

self.addEventListener('install', () => {
  // TODO(crbug.com/1005060): Move this outside the install event handler when the linked bug is fixed.
  setTimeout(() => {
    self.addEventListener('fetch', eventHandler);
  }, 0);
});
