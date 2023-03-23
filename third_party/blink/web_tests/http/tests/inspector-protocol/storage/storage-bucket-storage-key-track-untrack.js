(async function(testRunner) {
  const stabilizeNames =
      [...TestRunner.stabilizeNames, 'storageKey', 'bucketId', 'expiration'];
  const {dp, session} = await testRunner.startBlank(
      `Tests that tracking and untracking Storage Bucket for storage key works\n`);
  await dp.Page.enable();

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
                       frameId: frameId
                     })).result.storageKey;

  await dp.Storage.setStorageBucketTracking({storageKey, enable: true});

  {
    testRunner.log(`Create bucket`);

    const eventPromise = dp.Storage.onceStorageBucketCreatedOrUpdated();

    // Create bucket.
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          await navigator.storageBuckets.open("test-bucket");
          return 'bucket created successfully';
        } catch (err) {
          return err;
        }
      })()`);

    testRunner.log(result);
    const {params: {bucket}} = await eventPromise;
    testRunner.log(bucket, `Created bucket: `, stabilizeNames);
    if (bucket.expiration === 0) {
      testRunner.log('bucket.expiration equals zero\n');
    } else {
      testRunner.log(`bucket.expiration equals ${bucket.expiration}`);
    }
  }

  {
    testRunner.log(`Update bucket`);

    const eventPromise = dp.Storage.onceStorageBucketCreatedOrUpdated();

    // Update bucket.
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          await navigator.storageBuckets.open("test-bucket", {expires: Number.MAX_SAFE_INTEGER});
          return 'bucket updated successfully';
        } catch (err) {
          return err;
        }
      })()`);

    testRunner.log(result);
    const {params: {bucket}} = await eventPromise;
    testRunner.log(bucket, `Updated bucket: `, stabilizeNames);
    if (bucket.expiration !== 0) {
      testRunner.log('bucket.expiration does not equal zero\n');
    } else {
      testRunner.log(`bucket.expiration equals ${bucket.expiration}`);
    }
  }

  {
    testRunner.log(`Delete bucket`);

    const eventPromise = dp.Storage.onceStorageBucketDeleted();

    // Delete bucket.
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          await navigator.storageBuckets.delete("test-bucket");
          return 'bucket deleted successfully';
        } catch (err) {
          return err;
        }
      })()`);

    testRunner.log(result);
    const {params} = await eventPromise;
    testRunner.log(params, `Deleted bucket: `, stabilizeNames);
  }

  await dp.Storage.setStorageBucketTracking({storageKey, enable: false});

  {
    dp.Storage.onStorageBucketCreatedOrUpdated(
        message => {testRunner.log(message.params.bucket)});
    dp.Storage.onStorageBucketDeleted(
        message => {testRunner.log(message.params.bucketLocator)});

    testRunner.log('\nCreate another bucket after untracking.');

    // Create one more bucket.
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          await navigator.storageBuckets.open("test-bucket-2");
          return 'Another bucket opened successfully';
        } catch (err) {
          return err;
        }
      })()`);

    testRunner.log(result);
  }

  testRunner.completeTest();
})
