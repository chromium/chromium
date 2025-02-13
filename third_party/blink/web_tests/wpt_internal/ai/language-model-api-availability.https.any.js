promise_test(async t => {
  assert_true(!!ai);
  assert_not_equals(
    await ai.languageModel.availability(),
    'no'
  );
  assert_not_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["en"] }),
    'no'
  );
  assert_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["ja"] }),
    'no'
  );
});
