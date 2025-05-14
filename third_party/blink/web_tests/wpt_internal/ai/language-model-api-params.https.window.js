// META: title=Language Model Params
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  await ensureLanguageModel();

  const params = await LanguageModel.params();
  assert_true(!!params);
  assert_equals(typeof params.maxTopK, "number");
  assert_equals(typeof params.defaultTopK, "number");
  assert_equals(typeof params.maxTemperature, "number");
  assert_equals(typeof params.defaultTemperature, "number");
});
