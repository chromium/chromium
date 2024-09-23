// META: timeout=long

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  const result = await rewriter.rewrite('hello');
  assert_equals(typeof result, 'string');
}, 'Simple AIRewriter.rewrite() call');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  const streamingResponse = rewriter.rewriteStreaming('hello');
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
  const streamingResponse =
      rewriter.rewriteStreaming('hello', {signal: controller.signal});
  for await (const chunk of streamingResponse) {
  }
  controller.abort();
}, 'Aborting AIRewriter.rewriteStreaming() after finished reading');
