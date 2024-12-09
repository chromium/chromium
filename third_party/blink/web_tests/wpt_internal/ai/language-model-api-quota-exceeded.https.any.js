// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async t => {
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  // TODO(crbug.com/376789810): make it a PRECONDITION_FAILED if the model is
  // not ready.
  assert_true(status !== "no");
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
