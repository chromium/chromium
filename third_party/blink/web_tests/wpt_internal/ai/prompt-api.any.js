// META: script=resources/utils.js

promise_test(async () => {
  const result = await testPromptAPI();
  assert_true(result.success, result.error);
});
