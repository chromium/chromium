promise_test(async t => {
  // Make sure the model api is enabled.
  assert_true(!!model);
  // Make sure the session could be created.
  const status = await model.canCreateGenericSession();
  assert_true(status === 'readily');
  // Start a new session.
  const session = await model.createGenericSession();
  // Test the non streaming execute API.
  const response = await session.execute("What is 1+1?");
  assert_true(typeof response === "string");
  assert_true(response.length > 0);
});