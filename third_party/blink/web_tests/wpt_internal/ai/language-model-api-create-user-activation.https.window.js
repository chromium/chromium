// META: title=Language Model Create User Activation
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=resources/utils.js
// META: timeout=long

'use strict';

// Model download state is shared between test cases of the same file when run
// with `EchoAIManagerImpl`, so this test case needs to be on its own file.
promise_test(async t => {
  // Creating LanguageModel without user activation rejects with
  // NotAllowedError.
  await promise_rejects_dom(t, 'NotAllowedError', LanguageModel.create());

  // Creating LanguageModel with user activation succeeds.
  await createLanguageModel();

  // Creating it should have switched it to available.
  const availability = await LanguageModel.availability();
  assert_equals(availability, 'available');

  // Now that it is available, we should no longer need user activation.
  await LanguageModel.create();
}, 'Create requires user activation when availability is "downloadable.');