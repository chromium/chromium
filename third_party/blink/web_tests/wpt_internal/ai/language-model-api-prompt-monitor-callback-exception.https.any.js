// Test that the exception from the callback will be re-thrown by the session
// creation, and the session won't be created.
promise_test(async t => {
  // Make sure the model availability is `after-download`.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  if (status === "after-download") {
    const error = new Error("test");
    const sessionPromise = ai.languageModel.create({
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
