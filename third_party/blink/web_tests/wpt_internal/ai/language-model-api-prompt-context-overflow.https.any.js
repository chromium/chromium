// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async () => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await ai.languageModel.create();
  // Make sure there is something to evict.
  await session.prompt("Please write a sentence in English.");
  // Register the event listener.
  const promise = new Promise(resolve => {
    session.addEventListener("contextoverflow", () => {
      resolve(true);
    });
  });
  const promptString = await getPromptExceedingAvailableTokens(session);
  session.prompt(promptString);
  await promise;

  // Destroy the session here to stop the prompt, so that the next test can run
  // faster.
  session.destroy();
}, "event listener should be triggered when the context overflows.");
