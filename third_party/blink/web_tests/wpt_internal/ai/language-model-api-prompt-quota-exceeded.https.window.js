// META: title=Language Model Prompt Quota Exceeded
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await createLanguageModel();
  const promptString = await getPromptExceedingAvailableTokens(session);
  await promise_rejects_dom(t, "QuotaExceededError", session.prompt(promptString));
});
