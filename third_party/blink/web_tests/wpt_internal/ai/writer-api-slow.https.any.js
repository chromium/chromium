// META: timeout=long

const kTestInputString = 'hello';
// We need to have an English context string as Chrome's Rewriter API only
// supports English.
const kTestContextString = 'Hello world.';

promise_test(async () => {
  const writer = await ai.writer.create();
  const result =
      await writer.write(kTestInputString, {context: kTestContextString});
  assert_equals(typeof result, 'string');
}, 'Simple AIWriter.write() call');

promise_test(async () => {
  const writer = await ai.writer.create();
  const streamingResponse =
      writer.writeStreaming(kTestInputString, {context: kTestContextString});
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
  const streamingResponse = writer.writeStreaming(kTestInputString, {
    signal: controller.signal,
    context: kTestContextString,
  });
  for await (const chunk of streamingResponse) {
  }
  controller.abort();
}, 'Aborting AIWriter.writeStreaming() after finished reading');
