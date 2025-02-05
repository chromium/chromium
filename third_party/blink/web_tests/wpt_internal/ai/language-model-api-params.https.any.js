// META: script=resources/utils.js

promise_test(async () => {
  await ensureLanguageModel();

  const params = await ai.languageModel.params();
  assert_true(!!params);
  assert_true(typeof params.maxTopK === "number");
  assert_true(typeof params.defaultTopK === "number");
  assert_true(typeof params.maxTemperature === "number");
  assert_true(typeof params.defaultTemperature === "number");
});
