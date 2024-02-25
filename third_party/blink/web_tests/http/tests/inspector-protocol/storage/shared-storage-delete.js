(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage item deletion & clearing via devtools.`);

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
  let totalEventsSoFar = 0;

  async function getPromiseForEventCount(numEvents) {
    totalEventsSoFar += numEvents;
    return dp.Storage.onceSharedStorageAccessed(messageObject => {
      events.push(messageObject.params);
      return (events.length === totalEventsSoFar);
    });
  }

  await dp.Storage.setSharedStorageTracking({enable: true});

  eventPromise = getPromiseForEventCount(4);

  // The following calls should trigger events if shared storage is enabled, as
  // tracking is now enabled.
  //
  // Generates 4 events.
  await session.evaluateAsync(`
        sharedStorage.set('key0-set-from-document', 'value0');
        sharedStorage.set('key1-set-from-document', 'value1');
        sharedStorage.append('key1-set-from-document', 'value1');
        sharedStorage.set('key2-set-from-document', 'value2');
  `);

  // We wait to ensure that metadata and entries are done updating before we
  // retrieve them.
  await eventPromise;

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.log(`Delete an existing entry via devtools`);
  await dp.Storage.deleteSharedStorageEntry(
      {ownerOrigin: baseOrigin, key: 'key2-set-from-document'});

  testRunner.log(`Delete a non-existing entry via devtools`);
  await dp.Storage.deleteSharedStorageEntry(
      {ownerOrigin: baseOrigin, key: 'key3-set-from-document'});

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.log(`Clear entries via devtools`);
  await dp.Storage.clearSharedStorageEntries({ownerOrigin: baseOrigin});

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);
  await getSharedStorageEvents(testRunner, events);

  testRunner.completeTest();
})
