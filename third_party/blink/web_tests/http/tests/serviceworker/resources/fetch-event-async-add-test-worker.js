var eventHandler = async function (event) {
  event.respondWith(new Response('codeSupposedUnreachable'));
};

setTimeout(() => {
  self.addEventListener('fetch', eventHandler);
}, 1);
