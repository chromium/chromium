promise_test(async t => {
  // Make sure the model api is enabled.
  assert_true(!!model);
  // Make sure the session could be created.
  const status = await model.canCreateGenericSession();
  assert_true(status === 'readily');
  // Start a new session.
  const session = await model.createGenericSession();

  // Calling `session.destroy()` immediately after `session.execute()` will
  // trigger the "The model execution session has been destroyed." exception.
  let result = session.execute("What is 1+2?");
  session.destroy();
  await promise_rejects_dom(
    t, "InvalidStateError", result,
    "The model execution session has been destroyed."
  );

  // Calling `model.execute()` after `session.destroy()` will trigger the
  // "The model execution session has been destroyed." exception.
  await promise_rejects_dom(
    t, "InvalidStateError", session.execute("What is 2+3?"),
    "The model execution session has been destroyed."
  );
});
