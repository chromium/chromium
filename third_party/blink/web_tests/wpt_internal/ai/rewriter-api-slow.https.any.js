// META: timeout=long

const kTestInputString = 'hello';
// We need to have an English context string as Chrome's Rewriter API only
// supports English.
const kTestContextString = 'Hello world.';

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  const result =
      await rewriter.rewrite(kTestInputString, {context: kTestContextString});
  assert_equals(typeof result, 'string');
}, 'Simple AIRewriter.rewrite() call');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  const streamingResponse = rewriter.rewriteStreaming(
      kTestInputString, {context: kTestContextString});
  assert_equals(
      Object.prototype.toString.call(streamingResponse),
      '[object ReadableStream]');
  let result = '';
  for await (const chunk of streamingResponse) {
    result += chunk;
  }
  assert_true(result.length > 0);
}, 'Simple AIRewriter.rewriteStreaming() call');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  const streamingResponse = rewriter.rewriteStreaming(
      kTestInputString,
      {signal: controller.signal, context: kTestContextString});
  for await (const chunk of streamingResponse) {
  }
  controller.abort();
}, 'Aborting AIRewriter.rewriteStreaming() after finished reading');
