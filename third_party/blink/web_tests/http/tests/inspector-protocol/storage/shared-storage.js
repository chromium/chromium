(async function(testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage fetching of entries/metadata.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';

  async function getSharedStorageMetadata(dp, testRunner, origin) {
    const data = await dp.Storage.getSharedStorageMetadata(
      {ownerOrigin: origin});
    testRunner.log(data.result?.metadata, 'Metadata: ', ['creationTime']);
  }

  async function getSharedStorageEntries(dp, testRunner, origin) {
    const entriesResult = await dp.Storage.getSharedStorageEntries(
      {ownerOrigin: baseOrigin});
    testRunner.log(entriesResult.result?.entries, 'Entries:');
  }

  await session.evaluateAsync(`
        sharedStorage.set('key0-set-from-document', 'value0');
        sharedStorage.set('key1-set-from-document', 'value1');
        sharedStorage.append('key1-set-from-document', 'value1');
        sharedStorage.set('key2-set-from-document', 'value2');
  `);

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.log(`Clear entries`);
  await session.evaluateAsync('sharedStorage.clear()');

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.completeTest();
})
