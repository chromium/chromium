// META: title=Language Model Prompt Quota Exceeded
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await LanguageModel.create();
  const promptString = await getPromptExceedingAvailableTokens(session);
  await promise_rejects_dom(t, "QuotaExceededError", session.prompt(promptString));
});
