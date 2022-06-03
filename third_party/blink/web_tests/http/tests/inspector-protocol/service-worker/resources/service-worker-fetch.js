self.addEventListener('fetch', fetchEvent => {
  console.log(fetchEvent); // Should pause here.
  fetchEvent.respondWith(fetch(fetchEvent.request));
});
