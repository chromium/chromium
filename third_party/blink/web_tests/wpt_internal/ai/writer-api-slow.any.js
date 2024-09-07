// META: timeout=long

promise_test(async () => {
  const writer = await ai.writer.create();
  const result = await writer.write('hello');
  assert_equals(typeof result, 'string');
}, 'Simple AIWriter.write() call');

promise_test(async () => {
  const writer = await ai.writer.create();
  const streamingResponse = writer.writeStreaming('hello');
  assert_equals(
      Object.prototype.toString.call(streamingResponse),
      '[object ReadableStream]');
  let result = '';
  for await (const chunk of streamingResponse) {
    result += chunk;
  }
  assert_true(result.length > 0);
}, 'Simple AIWriter.writeStreaming() call');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  const controller = new AbortController();
  const streamingResponse =
      writer.writeStreaming('hello', {signal: controller.signal});
  for await (const chunk of streamingResponse) {
  }
  controller.abort();
}, 'Aborting AIWriter.writeStreaming() after finished reading');
