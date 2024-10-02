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

  // Test the countPromptTokens() API.
  let result = await session.countPromptTokens("This is a prompt.");
  assert_true(typeof result === "number");
});
