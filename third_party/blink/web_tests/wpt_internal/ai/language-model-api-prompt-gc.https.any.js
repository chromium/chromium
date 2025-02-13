// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async () => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await ai.languageModel.create();
  // Test the prompt API.
  const promptPromise = session.prompt(kTestPrompt);
  // Run GC.
  gc();
  const result = await promptPromise;
  assert_equals(typeof result, "string");
}, 'Prompt API must continue even after GC has been performed.');
