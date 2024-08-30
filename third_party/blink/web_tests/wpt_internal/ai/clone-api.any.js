// META: script=resources/utils.js
// META: script=resources/workaround-for-362676838.js
// META: timeout=long

promise_test(async () => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.assistant.capabilities();
  const status = capabilities.available;
  assert_true(status === 'readily');
  // Start a new session and test it.
  const session = await ai.assistant.create();
  let result = await testSession(session);
  assert_true(result.success, result.error);

  // Clone a session and test it.
  const cloned_session = await session.clone();
  assert_true(cloned_session.maxTokens === session.maxTokens);
  assert_true(cloned_session.tokensSoFar === session.tokensSoFar);
  assert_true(cloned_session.tokensLeft === session.tokensLeft);
  assert_true(cloned_session.topK === session.topK);
  assert_true(cloned_session.temperature === session.temperature);
  result = await testSession(cloned_session);
  assert_true(result.success, result.error);
});
