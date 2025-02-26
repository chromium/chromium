promise_test(async t => {
  assert_true(!!ai);
  assert_not_equals(
    await ai.languageModel.availability(),
    'unavailable'
  );
  assert_not_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["en"] }),
    'unavailable',
    'availability() with supported language should not return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ expectedInputLanguages: ["ja"] }),
    'unavailable',
    'availability() with unsupported language should return unavailable.'
  );
  assert_not_equals(
    await ai.languageModel.availability({ topK: 3, temperature: 0.5 }),
    'unavailable',
    'availability() with valid topK and temperature should not return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ topK: 0, temperature: 0.5 }),
    'unavailable',
    'availability() with zero topK should return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ topK: -3, temperature: 0.5 }),
    'unavailable',
    'availability() with negative topK should return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ topK: 3, temperature: -0.1 }),
    'unavailable',
    'availability() with negative temperature should return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ topK: 3 }),
    'unavailable',
    'availability() with only topK should return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({ temperature: 0.5 }),
    'unavailable',
    'availability() with only temperature should return unavailable.'
  );
  assert_not_equals(
    await ai.languageModel.availability({
      topK: 3,
      temperature: 1.5,
      expectedInputLanguages: ["en"]
    }),
    'unavailable',
    'availability() with valid sampling params and supported language should not return unavailable.'
  );
  assert_equals(
    await ai.languageModel.availability({
      topK: 3,
      temperature: -1,
      expectedInputLanguages: ["en"]
    }),
    'unavailable',
    'availability() with valid supported language and invalid smapling params should return unavailable.'
  );
});
