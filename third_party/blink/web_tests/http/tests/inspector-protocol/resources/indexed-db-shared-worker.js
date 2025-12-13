self.onconnect = function(e) {
  const port = e.ports[0];
  const request = indexedDB.open('TestDB_Shared', 1);
  request.onupgradeneeded = event => {
    const db = event.target.result;
    const store = db.createObjectStore('TestObjectStore', {keyPath: 'id'});
    store.put({id: 1, value: 'SharedWorkerValue'});
  };
  request.onsuccess = () => {
    port.postMessage('done');
  };
}
