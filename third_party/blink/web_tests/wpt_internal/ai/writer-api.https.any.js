// META: script=resources/utils.js

promise_test(async () => {
  assert_true(!!AIWriter);
}, 'AIWriter must be defined.');

promise_test(async () => {
  // TODO(crbug.com/382596381): Test availability with various options.
  assert_equals(await AIWriter.availability(), 'available');
  assert_equals(await AIWriter.availability({ outputLanguage: 'en' }), 'available');
}, 'AIWriter.availability');

promise_test(async () => {
  const writer = await AIWriter.create();
  assert_equals(Object.prototype.toString.call(writer), '[object AIWriter]');
}, 'AIWriter.create() must be return a AIWriter.');

promise_test(async () => {
  const writer = await AIWriter.create();
  assert_equals(writer.sharedContext, '');
  assert_equals(writer.tone, 'neutral');
  assert_equals(writer.format, 'plain-text');
  assert_equals(writer.length, 'medium');
}, 'AIWriter.create() default values.');

promise_test(async (t) => {
  const controller = new AbortController();
  controller.abort();
  const createPromise = AIWriter.create({signal: controller.signal});
  await promise_rejects_dom(t, 'AbortError', createPromise);
}, 'AIWriter.create() call with an aborted signal.');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const writer = await AIWriter.create({sharedContext: sharedContext});
  assert_equals(writer.sharedContext, sharedContext);
}, 'AIWriter.sharedContext');

promise_test(async () => {
  const writer = await AIWriter.create({tone: 'formal'});
  assert_equals(writer.tone, 'formal');
}, 'Creating a AIWriter with "formal" tone');

promise_test(async () => {
  const writer = await AIWriter.create({tone: 'casual'});
  assert_equals(writer.tone, 'casual');
}, 'Creating a AIWriter with "casual" tone');

promise_test(async () => {
  const writer = await AIWriter.create({format: 'markdown'});
  assert_equals(writer.format, 'markdown');
}, 'Creating a AIWriter with "markdown" format');

promise_test(async () => {
  const writer = await AIWriter.create({length: 'short'});
  assert_equals(writer.length, 'short');
}, 'Creating a AIWriter with "short" length');

promise_test(async () => {
  const writer = await AIWriter.create({length: 'long'});
  assert_equals(writer.length, 'long');
}, 'Creating a AIWriter with "long" length');

promise_test(async () => {
  const writer = await AIWriter.create({
    expectedInputLanguages: ['en']
  });
  assert_array_equals(writer.expectedInputLanguages, ['en']);
}, 'Creating a AIWriter with expectedInputLanguages');

promise_test(async () => {
  const writer = await AIWriter.create({
    expectedContextLanguages: ['en']
  });
  assert_array_equals(writer.expectedContextLanguages, ['en']);
}, 'Creating a AIWriter with expectedContextLanguages');

promise_test(async () => {
  const writer = await AIWriter.create({
    outputLanguage: 'en'
  });
  assert_equals(writer.outputLanguage, 'en');
}, 'Creating a AIWriter with outputLanguage');

promise_test(async () => {
  const writer = await AIWriter.create({});
  assert_equals(writer.expectedInputLanguages, null);
  assert_equals(writer.expectedContextLanguages, null);
  assert_equals(writer.outputLanguage, null);
}, 'Creating a AIWriter without optional attributes');

promise_test(async (t) => {
  const writer = await AIWriter.create();
  let result = await writer.write('');
  assert_equals(result, '');
  result = await writer.write(' ');
  assert_equals(result, '');
}, 'AIWriter.write() with an empty input or whitespace returns an empty text');

promise_test(async (t) => {
  const writer = await AIWriter.create();
  const result = await writer.write('hello', {context: ' '});
  assert_not_equals(result, '');
}, 'AIWriter.write() with a whitespace context returns a non-empty result');

promise_test(async (t) => {
  const writer = await AIWriter.create();
  writer.destroy();
  await promise_rejects_dom(t, 'InvalidStateError', writer.write('hello'));
}, 'AIWriter.write() fails after destroyed');

promise_test(async (t) => {
  const writer = await AIWriter.create();
  writer.destroy();
  assert_throws_dom('InvalidStateError', () => writer.writeStreaming('hello'));
}, 'AIWriter.writeStreaming() fails after destroyed');

promise_test(async () => {
  const writer = await AIWriter.create();
  const result = await writer.measureInputUsage(kTestPrompt);
  assert_greater_than(result, 0);
}, 'AIWriter.measureInputUsage() returns non-empty result');