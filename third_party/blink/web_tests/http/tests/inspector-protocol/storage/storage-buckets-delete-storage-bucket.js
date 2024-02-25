(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp, session } = await testRunner.startBlank(
    `Tests that deleting a bucket works correctly\n`);
  await dp.Page.enable();

  const stabilizeNames =
    [...TestRunner.stabilizeNames, 'storageKey', 'bucketId'];

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey =
    (await dp.Storage.getStorageKeyForFrame({ frameId })).result.storageKey;
  await dp.Storage.setStorageBucketTracking({ storageKey, enable: true });
  const bucketName = 'test-bucket';

  {
    testRunner.log(`Create bucket`);
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          await navigator.storageBuckets.open("${bucketName}");
          return 'buckets added successfully';
        } catch (err) {
          return err;
        }
      })()
    `);

    testRunner.log(result);
  }

  {
    testRunner.log(`Delete bucket`);

    dp.Storage.deleteStorageBucket({ bucket: { storageKey, name: bucketName } });
    const { params } = await dp.Storage.onceStorageBucketDeleted();
    testRunner.log(params, 'Deleted bucket: ', stabilizeNames);
  }

  testRunner.completeTest();
})
