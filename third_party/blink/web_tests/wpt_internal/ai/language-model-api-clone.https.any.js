// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async () => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  assert_true(status !== 'no');
  // Start a new session and test it.
  const session = await ai.languageModel.create();
  let result = await testSession(session);
  assert_true(result.success, result.error);

  // Clone a session and test it.
  const cloned_session = await session.clone();
  assert_true(
    cloned_session.maxTokens === session.maxTokens,
    'cloned session should have the same maxTokens as the original session.'
  );
  assert_true(
    cloned_session.tokensSoFar === session.tokensSoFar,
    'cloned session should have the same tokensSoFar as the original session.'
  );
  assert_true(
    cloned_session.tokensLeft === session.tokensLeft,
    'cloned session should have the same tokensLeft as the original session.'
  );
  assert_true(
    cloned_session.topK === session.topK,
    'cloned session should have the same topK as the original session.'
  );
  assert_true(
    cloned_session.temperature === session.temperature,
    'cloned session should have the same temperature as the original session.'
  );
  result = await testSession(cloned_session);
  assert_true(result.success, result.error);
});
