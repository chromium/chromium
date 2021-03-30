'use strict';

// TODO(ayui): This tests temporary behavior and should be removed when fully
// implemented. persist() should request for bucket persistence but not
// guarantee it.
promise_test(async testCase => {
  const bucket = await navigator.storageBuckets.open('bucket_name');
  testCase.add_cleanup(async () => {
    await navigator.storageBuckets.delete('bucket_name');
  });
  let persisted = await bucket.persisted();
  assert_false(persisted);

  await bucket.persist();

  persisted = await bucket.persisted();
  assert_true(persisted);
}, 'persist() should request persistance for bucket');

