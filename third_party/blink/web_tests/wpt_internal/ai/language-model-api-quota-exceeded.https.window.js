// META: title=Language Model Quota Exceeded
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session to get the max tokens.
  const session = await LanguageModel.create();
  const inputQuota = session.inputQuota;
  // Keep doubling an initial prompt until it exceeds the max tokens.
  let initialPrompt = "hello ";
  while (await session.measureInputUsage(initialPrompt) <= inputQuota) {
    initialPrompt += initialPrompt;
  }

  const promise = LanguageModel.create(
      { initialPrompts: [ { role: "system", content: initialPrompt } ] });
  await promise_rejects_dom(t, "QuotaExceededError", promise);
}, "QuotaExceededError is thrown when initial prompts are too large.");
