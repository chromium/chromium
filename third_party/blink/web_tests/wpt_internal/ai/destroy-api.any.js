// META: script=resources/workaround-for-362676838.js

promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.assistant.capabilities();
  const status = capabilities.available;
  assert_true(status === 'readily');
  // Start a new session.
  const session = await ai.assistant.create();

  // Calling `session.destroy()` immediately after `session.prompt()` will
  // trigger the "The model execution session has been destroyed." exception.
  let result = session.prompt("What is 1+2?");
  session.destroy();
  await promise_rejects_dom(
    t, "InvalidStateError", result,
    "The model execution session has been destroyed."
  );

  // Calling `session.prompt()` after `session.destroy()` will trigger the
  // "The model execution session has been destroyed." exception.
  await promise_rejects_dom(
    t, "InvalidStateError", session.prompt("What is 2+3?"),
    "The model execution session has been destroyed."
  );

  // After destroying the session, the properties should be still accessible.
  assert_true(typeof session.maxTokens === "number");
  assert_true(typeof session.tokensSoFar === "number");
  assert_true(typeof session.tokensLeft === "number");
  assert_true(typeof session.temperature === "number");
  assert_true(typeof session.topK === "number");
});
