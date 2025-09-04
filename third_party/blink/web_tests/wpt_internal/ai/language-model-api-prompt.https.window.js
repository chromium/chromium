// META: title=Language Model Prompt
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async () => {
  const result = await testPromptAPI();
  assert_true(result.success, result.error);
});

promise_test(async (t) => {
  await ensureLanguageModel();
  const session = await createLanguageModel();
  assert_true(!!(await session.prompt([])));
  // Invalid input should be stringified.
  assert_regexp_match(await session.prompt({}), /\[object Object\]/);
}, 'Check empty input');


promise_test(async (t) => {
  await ensureLanguageModel();
  const session = await createLanguageModel();
  assert_regexp_match(await session.prompt('shorthand'), /shorthand/);
  assert_regexp_match(
      await session.prompt([{role: 'system', content: 'shorthand'}]),
      /shorthand/);
}, 'Check Shorthand');

promise_test(async () => {
  const options = {
    initialPrompts:
        [{role: 'user', content: [{type: 'text', value: 'The word of the day is regurgitation.'}]}]
  };
  await ensureLanguageModel(options);
  const session = await LanguageModel.create(options);
  const tokenLength = await session.measureInputUsage(options.initialPrompts);
  assert_greater_than(tokenLength, 0);
  assert_equals(session.inputUsage, tokenLength);
  assert_regexp_match(
      await session.prompt([{role: 'system', content: ''}]),
      /regurgitation/);
}, 'Test that initialPrompt counts towards session inputUsage');
