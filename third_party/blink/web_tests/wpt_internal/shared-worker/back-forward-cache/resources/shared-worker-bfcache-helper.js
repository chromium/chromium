'use strict';

/**
 * Helper to retrieve a value from IndexedDB.
 * Returns a promise that resolves with the value if found, or undefined if
 * not found or on error.
 */
function getValueFromIndexedDB(dbName, storeName, key) {
  return new Promise((resolve) => {
    const req = indexedDB.open(dbName, 1);
    req.onerror = () => resolve(undefined);
    req.onsuccess = (e) => {
      const db = e.target.result;
      try {
        const tx = db.transaction(storeName, 'readonly');
        const getReq = tx.objectStore(storeName).get(key);
        getReq.onsuccess = (e) => resolve(e.target.result);
        getReq.onerror = () => resolve(undefined);
      } catch (e) {
        resolve(undefined);
      }
    };
  });
}
