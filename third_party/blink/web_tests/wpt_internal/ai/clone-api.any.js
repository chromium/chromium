// META: script=resources/utils.js

promise_test(async () => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const status = await ai.canCreateTextSession();
  assert_true(status === 'readily');
  // Start a new session and test it.
  const session = await ai.createTextSession();
  let result = await testSession(session);
  assert_true(result.success, result.error);

  // Clone a session and test it.
  const cloned_session = await session.clone();
  result = await testSession(cloned_session);
  assert_true(result.success, result.error);
});
