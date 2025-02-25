// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async t => {
  await ensureLanguageModel();
}, 'Ensure sessions can be created');

promise_test(async t => {
  let session = await ai.languageModel.create();
  assert_true(!!session);
}, 'Create with no options');

promise_test(async t => {
  let session = await ai.languageModel.create({ topK: 3, temperature: 0.6 });
  assert_true(!!session);
}, 'Create with topK and temperature');

promise_test(async t => {
  let result = ai.languageModel.create({ topK: 3 });
  await promise_rejects_dom(
    t, 'NotSupportedError', result,
    'Initializing a new session must either specify both topK and temperature, or neither of them.');
}, 'Create with only topK should fail');

promise_test(async t => {
  let result = ai.languageModel.create({ temperature: 0.5 });
  await promise_rejects_dom(
    t, 'NotSupportedError', result,
    'Initializing a new session must either specify both topK and temperature, or neither of them.');
}, 'Create with only temperature should fail');

promise_test(async t => {
  let result = ai.languageModel.create({ topK: 3, temperature: -0.5 });
  await promise_rejects_js(t, RangeError, result);
}, 'Create with negative temperature should fail');

promise_test(async t => {
  let result = ai.languageModel.create({ topK: 0, temperature: 0.5 });
  await promise_rejects_js(t, RangeError, result);
}, 'Create with zero topK should fail');

promise_test(async t => {
  let result = ai.languageModel.create({ topK: -2, temperature: 0.5 });
  await promise_rejects_js(t, RangeError, result);
}, 'Create with negative topK should fail');

promise_test(async t => {
  let session = await ai.languageModel.create({ topK: 1.5, temperature: 0.5 });
  assert_true(!!session);
  assert_equals(session.topK, 1);
}, 'Create with fractional topK should be rounded down');

promise_test(async t => {
  let session = await ai.languageModel.create({ systemPrompt: 'you are a robot' });
  assert_true(!!session);
}, 'Create with systemPrompt');

promise_test(async t => {
  let session = await ai.languageModel.create({
    initialPrompts: [
      {role: 'system', content: 'you are a robot'},
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  assert_true(!!session);
}, 'Create with initialPrompts');

promise_test(async t => {
  let session = await ai.languageModel.create({
    initialPrompts: [
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  assert_true(!!session);
}, 'Create with initialPrompts without system role');

promise_test(async t => {
  let result = ai.languageModel.create({
    initialPrompts: [
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'},
      {role: 'system', content: 'you are a robot'}
    ]
  });
  await promise_rejects_js(t, TypeError, result);
}, 'Create with system role not ordered first should fail');

promise_test(async t => {
  let session = await ai.languageModel.create({
    systemPrompt: 'you are a robot',
    initialPrompts: [
      { role: 'user', content: 'hello' }, { role: 'assistant', content: 'hello' }
    ]
  });
  assert_true(!!session);
}, 'Create with systemPrompt and non-system initialPrompts');

promise_test(async t => {
  let result = ai.languageModel.create({
    systemPrompt: 'you are a robot',
    initialPrompts: [
      {role: 'system', content: 'you are a robot'},
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  await promise_rejects_js(t, TypeError, result);
}, 'Create with systemPrompt and system initialPrompts should fail');
