// META: title=Language Model Prompt
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  const result = await testPromptAPI();
  assert_true(result.success, result.error);
});
