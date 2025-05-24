// META: title=Language Model Prompt Monitor Callback Exception
// META: script=resources/utils.js
// META: timeout=long

'use strict';

// Test that the exception from the callback will be re-thrown by the session
// creation, and the session won't be created.
promise_test(async t => {
  // Make sure the model availability is `downloadable`.
  const availability = await LanguageModel.availability();
  if (availability === "downloadable") {
    const error = new Error("test");
    const sessionPromise = LanguageModel.create({
      // Start a new session with callback that will throw error.
      monitor(m) {
        throw error;
      }
    });
    await promise_rejects_exactly(
      t, error, sessionPromise
    );
  }
});
