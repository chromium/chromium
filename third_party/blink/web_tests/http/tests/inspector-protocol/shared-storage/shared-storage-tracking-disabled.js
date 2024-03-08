(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests disabling of shared storage event tracking.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';

  async function getSharedStorageMetadata(dp, testRunner, origin) {
    const data =
        await dp.Storage.getSharedStorageMetadata({ownerOrigin: origin});
    testRunner.log(data.result?.metadata, 'Metadata: ', ['creationTime']);
  }

  async function getSharedStorageEntries(dp, testRunner, origin) {
    const entriesResult =
        await dp.Storage.getSharedStorageEntries({ownerOrigin: baseOrigin});
    testRunner.log(entriesResult.result?.entries, 'Entries:');
  }

  async function getSharedStorageEvents(testRunner, events) {
    testRunner.log(events, 'Events: ', ['accessTime', 'mainFrameId']);
  }

  const events = [];
  dp.Storage.onSharedStorageAccessed(
      (messageObject) => {events.push(messageObject.params)});
  await dp.Storage.setSharedStorageTracking({enable: false});

  // This call should not trigger any events, since tracking is disabled.
  await session.evaluateAsync(`
        sharedStorage.set('key0-set-from-document', 'value0');
        sharedStorage.set('key1-set-from-document', 'value1');
        sharedStorage.append('key1-set-from-document', 'value1');
        sharedStorage.set('key2-set-from-document', 'value2');
        sharedStorage.delete('key2-set-from-document');
  `);

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  // We do not expect any events.
  await getSharedStorageEvents(testRunner, events);

  testRunner.completeTest();
})
