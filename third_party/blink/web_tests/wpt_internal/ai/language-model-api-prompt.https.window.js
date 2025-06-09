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
  promise_rejects_dom(t, 'TypeError', session.prompt([]));
  promise_rejects_dom(t, 'TypeError', session.prompt({}));
}, 'Check malformed input');

promise_test(async (t) => {
  await ensureLanguageModel();
  const session = await createLanguageModel();
  assert_regexp_match(await session.prompt('shorthand'), /shorthand/);
  assert_regexp_match(
      await session.prompt([{role: 'system', content: 'shorthand'}]),
      /shorthand/);
}, 'Check Shorthand');
