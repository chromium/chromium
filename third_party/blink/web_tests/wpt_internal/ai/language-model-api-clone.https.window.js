// META: title=Language Model Clone
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  await ensureLanguageModel();

  // Start a new session and test it.
  const session = await LanguageModel.create();
  let result = await testSession(session);
  assert_true(result.success, result.error);

  // Clone a session and test it.
  const cloned_session = await session.clone();
  assert_equals(
    cloned_session.inputQuota, session.inputQuota,
    'cloned session should have the same inputQuota as the original session.'
  );
  assert_equals(
    cloned_session.inputUsage, session.inputUsage,
    'cloned session should have the same inputUsage as the original session.'
  );
  assert_equals(
    cloned_session.topK, session.topK,
    'cloned session should have the same topK as the original session.'
  );
  assert_equals(
    cloned_session.temperature, session.temperature,
    'cloned session should have the same temperature as the original session.'
  );
  result = await testSession(cloned_session);
  assert_true(result.success, result.error);
});
