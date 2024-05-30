promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const status = await ai.canCreateTextSession();
  assert_true(status === 'readily');
  // Start a new session.
  const session = await ai.createTextSession();
  // Test the non streaming prompt API.
  const response = await session.prompt("What is 1+1?");
  assert_true(typeof response === "string");
  assert_true(response.length > 0);
});