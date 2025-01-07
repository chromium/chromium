// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js

promise_test(async t => {
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  // TODO(crbug.com/376789810): make it a PRECONDITION_FAILED if the model is
  // not ready.
  assert_true(status !== "no");
  // Start a new session.
  const session = await ai.languageModel.create();
  const promptString = await getPromptExceedingAvailableTokens(session);
  await promise_rejects_dom(t, "QuotaExceededError", session.prompt(promptString));
});
