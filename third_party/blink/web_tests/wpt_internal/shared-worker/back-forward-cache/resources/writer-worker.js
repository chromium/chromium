self.onconnect = event => {
  const port = event.ports[0];
  port.onmessage = async (msg) => {
    if (msg.data.command === 'try_to_write') {
      const request = self.indexedDB.open(msg.data.dbName, 1);
      const storeName = msg.data.storeName
      request.onupgradeneeded = e =>
          e.target.result.createObjectStore(storeName);
      request.onsuccess = e => {
        const tx = e.target.result.transaction(storeName, 'readwrite');
        tx.objectStore(storeName).put('value', 'key');
      };
    }
  };
};
