// This worker is used to test that a shared worker can write to the DB during
// the client page is in BFCache.
const bc = new BroadcastChannel('shared-worker-bfcache-test');
self.onconnect = event => {
  bc.onmessage = async (msg) => {
    if (msg.data.command === 'try_to_write') {
      const request = self.indexedDB.open(msg.data.dbName, 1);
      const storeName = msg.data.storeName;
      request.onupgradeneeded = e =>
          e.target.result.createObjectStore(storeName);
      request.onsuccess = e => {
        const tx = e.target.result.transaction(storeName, 'readwrite');
        tx.objectStore(storeName).put(msg.data.value, msg.data.key);
        tx.oncomplete = () => {
          bc.postMessage('wrote_to_db');
        };
      };
    }
  };
};
