promise_test(async () => {
  assert_true(!!ai);
  assert_true(!!ai.writer);
  assert_equals(
      Object.prototype.toString.call(ai.writer), '[object AIWriterFactory]');
}, 'AIWriterFactory must be available.');

promise_test(async () => {
  const writer = await ai.writer.create();
  assert_equals(Object.prototype.toString.call(writer), '[object AIWriter]');
}, 'AIWriterFactory.create() must be return a AIWriter.');

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

