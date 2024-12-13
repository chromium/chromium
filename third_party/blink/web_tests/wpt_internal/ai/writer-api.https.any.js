promise_test(async () => {
  assert_true(!!ai);
  assert_true(!!ai.writer);
  assert_equals(
      Object.prototype.toString.call(ai.writer), '[object AIWriterFactory]');
}, 'AIWriterFactory must be available.');

promise_test(async () => {
  // TODO(crbug.com/382596381): Test availability with various options.
  assert_equals(await ai.writer.availability(), 'readily');
  assert_equals(await ai.writer.availability({outputLanguage: 'en'}), 'readily');
}, 'AIWriterFactory.availability');

promise_test(async () => {
  const writer = await ai.writer.create();
  assert_equals(Object.prototype.toString.call(writer), '[object AIWriter]');
}, 'AIWriterFactory.create() must be return a AIWriter.');

promise_test(async () => {
  const writer = await ai.writer.create();
  assert_equals(writer.sharedContext, '');
  assert_equals(writer.tone, 'neutral');
  assert_equals(writer.format, 'plain-text');
  assert_equals(writer.length, 'medium');
}, 'AIWriterFactory.create() default values.');

promise_test(async (t) => {
  const controller = new AbortController();
  controller.abort();
  const createPromise = ai.writer.create({signal: controller.signal});
  await promise_rejects_dom(t, 'AbortError', createPromise);
}, 'AIWriterFactory.create() call with an aborted signal.');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const writer = await ai.writer.create({sharedContext: sharedContext});
  assert_equals(writer.sharedContext, sharedContext);
}, 'AIWriter.sharedContext');

promise_test(async () => {
  const writer = await ai.writer.create({tone: 'formal'});
  assert_equals(writer.tone, 'formal');
}, 'Creating a AIWriter with "formal" tone');

promise_test(async () => {
  const writer = await ai.writer.create({tone: 'casual'});
  assert_equals(writer.tone, 'casual');
}, 'Creating a AIWriter with "casual" tone');

promise_test(async () => {
  const writer = await ai.writer.create({format: 'markdown'});
  assert_equals(writer.format, 'markdown');
}, 'Creating a AIWriter with "markdown" format');

promise_test(async () => {
  const writer = await ai.writer.create({length: 'short'});
  assert_equals(writer.length, 'short');
}, 'Creating a AIWriter with "short" length');

promise_test(async () => {
  const writer = await ai.writer.create({length: 'long'});
  assert_equals(writer.length, 'long');
}, 'Creating a AIWriter with "long" length');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  writer.destroy();
  await promise_rejects_dom(t, 'InvalidStateError', writer.write('hello'));
}, 'AIWriter.write() fails after destroyed');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  writer.destroy();
  assert_throws_dom('InvalidStateError', () => writer.writeStreaming('hello'));
}, 'AIWriter.writeStreaming() fails after destroyed');

