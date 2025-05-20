// META: title=Language Model Availability Available
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  await ensureLanguageModel();
}, 'LanguageModel.availability() is available with no options');

promise_test(async () => {
  await ensureLanguageModel();
  // An array of supported test option values.
  const kCreateOptionsSpec = [
    {topK: [1, 1.5, 2, 3, 99]},  // Nominally int 1-10+.
    {temperature: [0, 0.5, 1, 2]},  // Nominally float 0-1.
    {expectedInputs: [undefined, [], [{type: 'text'}], [{type: 'text', languages: ['en']}], ]},
  ];
  for (const options of generateOptionCombinations(kCreateOptionsSpec)) {
    const availability = await LanguageModel.availability(options);
    assert_in_array(availability, kValidAvailabilities, options);
  }
}, 'LanguageModel.availability() returns available with supported options');

promise_test(async () => {
  await ensureLanguageModel();
  // An array of unsupported test options.
  const kUnsupportedCreateOptions = [
    { expectedInputs: [{type: 'text', languages: ['unk']}] },  // Language not supported.
    { topK: 0, temperature: 0.5 },  // zero topK not supported.
    { topK: -3, temperature: 0.5 },  // negative topK not supported.
    { topK: 3, temperature: -0.5 },  // negative temperature not supported.
    { topK: 3 },  // topK without temperature not supported.
    { temperature: 0.5 },  // temperature without topK not supported.
  ];
  for (const options of kUnsupportedCreateOptions) {
    assert_equals(await LanguageModel.availability(options), 'unavailable', options);
  }
}, 'LanguageModel.availability() returns unavailable with unsupported options');

promise_test(async t => {
  await ensureLanguageModel();
  // An array of invalid test options.
  const kInvalidCreateOptions = [
    { expectedInputs: [{type: 'soup'}]},  // Type not supported.
  ];
  for (const options of kInvalidCreateOptions) {
    await promise_rejects_js(t, TypeError, LanguageModel.availability(options));
  }
}, 'LanguageModel.availability() rejects with invalid options');
