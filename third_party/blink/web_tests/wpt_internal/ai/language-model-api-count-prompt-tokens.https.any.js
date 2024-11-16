// META: script=resources/workaround-for-362676838.js

promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  assert_true(status !== "no");
  // Start a new session.
  const session = await ai.languageModel.create();

  // Test the countPromptTokens() API.
  let result = await session.countPromptTokens("This is a prompt.");
  assert_true(
    typeof result === "number" && result > 0,
    "The counting result should be a positive number."
  );
});
