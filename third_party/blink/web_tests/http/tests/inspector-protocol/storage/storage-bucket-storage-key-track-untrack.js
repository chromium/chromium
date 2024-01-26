(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const stabilizeNames =
    [...TestRunner.stabilizeNames, 'storageKey', 'bucketId', 'expiration'];
  const { dp, session } = await testRunner.startBlank(
    `Tests that tracking and untracking Storage Bucket for storage key works\n`);
  await dp.Page.enable();

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
    frameId: frameId
  })).result.storageKey;

  await dp.Storage.setStorageBucketTracking({ storageKey, enable: true });

  {
    testRunner.log(`Create bucket`);

    // Note that we could also get an event for creation of the default bucket.
    // Let us filter the events to our "test-bucket".
    const eventPromise = dp.Storage.onceStorageBucketCreatedOrUpdated(
      event => event.params.bucketInfo.bucket.name === 'test-bucket');

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
    const { params: { bucketInfo } } = await eventPromise;
    testRunner.log(bucketInfo, `Created bucket: `, stabilizeNames);
    if (bucketInfo.expiration === 0) {
      testRunner.log('bucket.expiration equals zero\n');
    } else {
      testRunner.log(`bucket.expiration equals ${bucket.expiration}`);
    }
  }

  {
    testRunner.log(`Update bucket`);

    const eventPromise = dp.Storage.onceStorageBucketCreatedOrUpdated(
      event => event.params.bucketInfo.bucket.name === 'test-bucket');

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
    const { params: { bucketInfo } } = await eventPromise;
    testRunner.log(bucketInfo, `Updated bucket: `, stabilizeNames);
    if (bucketInfo.expiration !== 0) {
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
    const { params } = await eventPromise;
    testRunner.log(params, `Deleted bucket: `, stabilizeNames);
  }

  await dp.Storage.setStorageBucketTracking({ storageKey, enable: false });

  {
    dp.Storage.onStorageBucketCreatedOrUpdated(
      message => { testRunner.log(message.params.bucket) });
    dp.Storage.onStorageBucketDeleted(
      message => { testRunner.log(message.params.bucketLocator) });

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
