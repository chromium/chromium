// META: title=Blob Valid After concurrent txn abort
// META: global=window,worker
// META: script=/IndexedDB/resources/support.js

let key = 'key';

// This is an internal test because it relies on transactions being run
// concurrently, which is not technically required by the spec.
indexeddb_test(
  function upgrade(t, db) {
    db.createObjectStore('store');
    db.createObjectStore('logs');
  },
  function success(t, db) {
    const blobAContent = 'Blob A content';
    const blobA = new Blob([blobAContent], { 'type': 'text/plain' });
    const value = { a0: blobA };

    // Begin by writing a blob and committing.
    const txn = db.transaction('store', 'readwrite')
    txn.objectStore('store').put(value, key);
    txn.oncomplete = t.step_func(() => {
      // Start a new RW transaction.
      const txn2 = db.transaction('logs', 'readwrite');
      txn2.onsuccess = t.unreached_func();
      let readBlob;
      // Keep it alive while the readonly transaction executes.
      const doWrite = () => {
        if (readBlob) {
          return;
        }
        const writeRequest = txn2.objectStore('logs').put('hello', 'world');
        writeRequest.onsuccess = doWrite;
      };
      doWrite();

      // Start a RO transaction which can be concurrent since it has non-
      // overlapping scope.
      const ro_txn = db.transaction('store', 'readonly');
      const store = ro_txn.objectStore('store');
      const request = store.get(key);
      // Get the blob in this transaction, but don't read it yet.
      request.onsuccess = () => {
        readBlob = request.result.a0;
        // Abort the concurrent RW transaction. If the implementation is not
        // careful, this could invalidate the reference added for the active
        // blob.
        txn2.abort();
      };

      // Now delete the blob from the database, then try to read it. The
      // existence of the blob reference should keep the bytes alive and
      // readable.
      const txn3 = db.transaction('store', 'readwrite');
      const writeRequest3 = txn3.objectStore('store').put('overwrite', key);
      writeRequest3.onsuccess = t.step_func(() => {
        readBlob.text().then(
          t.step_func_done(text => {
            assert_equals(text, blobAContent);
          }),
          t.unreached_func());
      });
    });
  },
  'The blob reference returned by a RO transaction should stay valid even if a concurrent RW transaction is aborted');
