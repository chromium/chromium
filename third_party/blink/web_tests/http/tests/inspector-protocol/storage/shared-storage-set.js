(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage item setting via devtools.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';

  await dp.Storage.clearSharedStorageEntries({ownerOrigin: baseOrigin});

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

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  testRunner.log(`Set entries via devtools`);
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key0-set-from-devtools',
    value: 'value0'
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key1-set-from-devtools',
    value: 'value1',
    ignoreIfPresent: false
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key2-set-from-devtools',
    value: 'value2',
    ignoreIfPresent: true
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key3-set-from-devtools',
    value: 'value3',
    ignoreIfPresent: false
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key3-set-from-devtools',
    value: 'ignored',
    ignoreIfPresent: true
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key4-set-from-devtools',
    value: 'value4',
    ignoreIfPresent: true
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key4-set-from-devtools',
    value: 'overridden',
    ignoreIfPresent: false
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key5-set-from-devtools',
    value: 'value5'
  });
  await dp.Storage.setSharedStorageEntry({
    ownerOrigin: baseOrigin,
    key: 'key5-set-from-devtools',
    value: 'overridden'
  });

  await getSharedStorageMetadata(dp, testRunner, baseOrigin);
  await getSharedStorageEntries(dp, testRunner, baseOrigin);

  await dp.Storage.clearSharedStorageEntries({ownerOrigin: baseOrigin});

  testRunner.completeTest();
})
