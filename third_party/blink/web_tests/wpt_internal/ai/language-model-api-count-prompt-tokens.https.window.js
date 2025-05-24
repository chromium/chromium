// META: title=Language Model Measure Input Usage
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await LanguageModel.create();

  // Test the measureInputUsage() API.
  let result = await session.measureInputUsage("This is a prompt.");
  assert_true(
    typeof result === "number" && result > 0,
    "The counting result should be a positive number."
  );
});
