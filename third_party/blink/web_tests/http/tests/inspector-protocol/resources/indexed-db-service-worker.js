self.addEventListener('install', event => {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim().then(() => {
    return new Promise(resolve => {
      const request = indexedDB.open('TestDB_Service', 1);
      request.onupgradeneeded = e => {
        const db = e.target.result;
        const store = db.createObjectStore('TestObjectStore', {keyPath: 'id'});
        store.put({id: 1, value: 'ServiceWorkerValue'});
      };
      request.onsuccess = () => {
        self.clients.matchAll({type: 'window'}).then(clients => {
          for (const client of clients) {
            client.postMessage('done');
          }
        });
        resolve();
      };
      request.onerror = () => resolve();
    });
  }));
});
