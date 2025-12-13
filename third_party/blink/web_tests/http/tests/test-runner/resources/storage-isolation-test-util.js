(function() {
async function createAllData(identifier) {
  localStorage.setItem(`key_${identifier}`, 'data');
  await new Promise(r => indexedDB.open(`db_${identifier}`, 1).onsuccess = r);
  await caches.open(`cache_${identifier}`);

  const opfsRoot = await navigator.storage.getDirectory();
  const fileHandle =
      await opfsRoot.getFileHandle(`file_${identifier}.txt`, {create: true});
  const writable = await fileHandle.createWritable();
  await writable.write('data');
  await writable.close();
}

async function verifyStorageIsEmptyAndLogDetails() {
  let isOverallClean = true;
  function check(isClean, cleanMessage, dirtyMessage) {
    if (isClean) {
      console.log(`  OK: ${cleanMessage}`);
    } else {
      console.log(`  FAIL: ${dirtyMessage}`);
      isOverallClean = false;
    }
  }

  check(
      localStorage.length === 0, 'LocalStorage is empty.',
      `LocalStorage is not empty. Found ${localStorage.length} items.`);
  const idbDatabases = await indexedDB.databases();
  check(
      idbDatabases.length === 0, 'IndexedDB is empty.',
      `IndexedDB is not empty. Found ${idbDatabases.length} databases.`);
  const cacheKeys = await caches.keys();
  check(
      cacheKeys.length === 0, 'CacheStorage is empty.',
      `CacheStorage is not empty. Found ${cacheKeys.length} caches.`);

  const opfsEntries = await (async () => {
    const entries = [];
    for await (
        const entry of (await navigator.storage.getDirectory()).values()) {
      entries.push(entry.name);
    }
    return entries;
  })();
  check(
      opfsEntries.length === 0, 'Origin Private File System is empty.',
      `Origin Private File System is not empty.`);

  return isOverallClean;
}

window.runStorageIsolationTest = async function(testFileName) {
  console.log(`--- [${testFileName}] Starting Test ---`);
  console.log('Verifying initial state...');
  await verifyStorageIsEmptyAndLogDetails();

  console.log('Creating data...');
  await createAllData(testFileName);

  console.log(`--- [${testFileName}] Test Finished ---`);
};
})();
