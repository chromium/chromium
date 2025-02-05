// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session to get the max tokens.
  const session = await ai.languageModel.create();
  const maxTokens = session.maxTokens;
  // Keep doubling the system prompt until it exceeds the maxTokens.
  let systemPrompt = "hello ";
  while (await session.countPromptTokens(systemPrompt) <= maxTokens) {
    systemPrompt += systemPrompt;
  }

  const promise = ai.languageModel.create({ systemPrompt: systemPrompt });
  await promise_rejects_dom(t, "QuotaExceededError", promise);
}, "QuotaExceededError should be thrown if the system prompt is too large.");
