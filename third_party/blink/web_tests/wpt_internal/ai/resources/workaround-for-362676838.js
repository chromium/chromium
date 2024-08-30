// To mitigates the issue where existing sessions are aborted during language
// detection model loading, this script introduces a test step to wait for
// AIWriter initialization, which depends on the language detection model.
// This ensures the language detection model is fully loaded before any tests
// which don't use the language detection model, preventing test flakiness.
// TODO(crbug.com/362676838): Remove this when the issue is fixed.
promise_test(async () => {
  const writer = await ai.writer.create();
  writer.destroy();
}, 'A workaround step for crbug.com/362676838');
