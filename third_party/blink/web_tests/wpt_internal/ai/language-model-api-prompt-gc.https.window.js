// META: title=Language Model Prompt GC
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await LanguageModel.create();
  // Test the prompt API.
  const promptPromise = session.prompt(kTestPrompt);
  // Run GC.
  gc();
  const result = await promptPromise;
  assert_equals(typeof result, "string");
}, 'Prompt API must continue even after GC has been performed.');
