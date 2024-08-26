// META: script=resources/utils.js
// META: timeout=long

promise_test(async () => {
  const result = await testPromptAPI();
  assert_true(result.success, result.error);
});
