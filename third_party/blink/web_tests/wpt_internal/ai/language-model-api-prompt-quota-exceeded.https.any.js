// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await ai.languageModel.create();
  const promptString = await getPromptExceedingAvailableTokens(session);
  await promise_rejects_dom(t, "QuotaExceededError", session.prompt(promptString));
});
