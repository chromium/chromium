// META: title=Buckets API: Tests for the StorageBucket object.
// META: global=window,worker

'use strict';

// This test is for the initial version of the StorageBucket object for
// debugging.
//
// TODO(ayui): Split and add extensive testing for each endpoint after endpoints
// are fully implemented.
promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });
  const persisted = await bucket.persisted();
  assert_false(persisted);
}, 'persisted() should default to false');

// TODO(ayui): This tests temporary behavior and should be removed when fully
// implemented. estimate() should return actual usage metrics but currently does
// not.
promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });
  const estimate = await bucket.estimate();
  assert_equals(estimate.quota, 0);
  assert_equals(estimate.usage, 0);
}, 'estimate() should retrieve quota usage');

promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open(
      'bucket_name', { durability: 'strict' });
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });

  const durability = await bucket.durability();
  assert_equals('strict', durability);
}, 'durability() should retrieve bucket durability specified during creation');

promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });

  const durability = await bucket.durability();
  assert_equals('relaxed', durability);
}, 'Bucket durability defaults to relaxed');

promise_test(async testCase => {
  const oneYear = 365 * 24 * 60 * 60 * 1000;
  const expiresDate = Date.now() + oneYear;
  const bucket = await navigator.storageBuckets.open(
      'bucket_name', { expires: expiresDate });
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });

  const expires = await bucket.expires();
  assert_equals(expires, expiresDate);
}, 'expires() should retrieve expires date');

promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });

  const expires = await bucket.expires();
  assert_equals(expires, null);
}, 'expires() should be defaulted to null');

promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });

  const oneYear = 365 * 24 * 60 * 60 * 1000;
  const expiresDate = Date.now() + oneYear;
  await bucket.setExpires(expiresDate);

  const expires = await bucket.expires();
  assert_equals(expires, expiresDate);
}, 'setExpires() should set bucket expires date');

promise_test(async testCase => {
  const oneDay = 24 * 60 * 60 * 1000;
  const expiresDate = Date.now() + oneDay;
  const bucket = await navigator.storageBuckets.open('bucket_name', {
    expires: expiresDate
  });
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });
  let expires = await bucket.expires();
  assert_equals(expires, expiresDate);

  const oneYear = 365 * oneDay;
  const newExpiresDate = Date.now() + oneYear;
  await bucket.setExpires(newExpiresDate);

  expires = await bucket.expires();
  assert_equals(expires, newExpiresDate);
}, 'setExpires() should update expires date');
