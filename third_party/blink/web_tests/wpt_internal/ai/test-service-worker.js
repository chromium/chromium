self.importScripts("resources/utils.js");

self.onactivate = async e => {
  e.waitUntil(
    self.clients.claim().then(() => {
      self.clients.matchAll().then(async clients => {
        let port = clients[0];
        port.postMessage(await testPromptAPI());
      });
    })
  );
};
