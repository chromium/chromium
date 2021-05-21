self.addEventListener('fetch', e => {
  if (e.request.url.includes('should-intercept'))
    e.respondWith(new Response('intercepted by service worker'));
});
