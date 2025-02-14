promise_test(async t => {
  assert_true(!!ai);
  assert_not_equals(
    await ai.languageModel.availability(),
    'unavailable'
  );
  assert_not_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["en"] }),
    'unavailable'
  );
  assert_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["ja"] }),
    'unavailable'
  );
});
