const request = indexedDB.open('TestDB_Dedicated', 1);
request.onupgradeneeded = e => {
  const db = e.target.result;
  const store = db.createObjectStore('TestObjectStore', {keyPath: 'id'});
  store.put({id: 1, value: 'DedicatedWorkerValue'});
};
request.onsuccess = () => {
  self.postMessage('done');
};
