// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async () => {
  await ensureLanguageModel();

  // Start a new session and test it.
  const session = await ai.languageModel.create();
  let result = await testSession(session);
  assert_true(result.success, result.error);

  // Clone a session and test it.
  const cloned_session = await session.clone();
  assert_equals(
    cloned_session.maxTokens, session.maxTokens,
    'cloned session should have the same maxTokens as the original session.'
  );
  assert_equals(
    cloned_session.tokensSoFar, session.tokensSoFar,
    'cloned session should have the same tokensSoFar as the original session.'
  );
  assert_equals(
    cloned_session.tokensLeft, session.tokensLeft,
    'cloned session should have the same tokensLeft as the original session.'
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
