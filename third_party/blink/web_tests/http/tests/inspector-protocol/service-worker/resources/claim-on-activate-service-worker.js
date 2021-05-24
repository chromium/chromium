(function () {
  self.addEventListener("activate", onActivate);
  self.addEventListener("fetch", onFetch);

  function onActivate(e) {
    e.waitUntil(
      Promise.resolve().then(
        () => self.clients.claim()
      ));
  };

  async function onFetch(e) {
    if (e.request.mode === "navigate")
      return;
    e.respondWith(fetch(e.request));
  }
})();
