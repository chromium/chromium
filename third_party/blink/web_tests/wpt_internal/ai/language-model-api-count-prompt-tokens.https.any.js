// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await ai.languageModel.create();

  // Test the countPromptTokens() API.
  let result = await session.countPromptTokens("This is a prompt.");
  assert_true(
    typeof result === "number" && result > 0,
    "The counting result should be a positive number."
  );
});
