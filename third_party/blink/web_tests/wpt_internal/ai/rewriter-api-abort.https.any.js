// META: script=resources/workaround-for-362676838.js
// META: script=resources/utils.js

promise_test(async (t) => {
  testAbort(t, (signal) => {
    return ai.rewriter.create({
      signal: signal
    });
  });
}, 'Aborting AIRewriter.create().');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  testAbort(t, (signal) => {
    return rewriter.rewrite(
      "Rewrite a poem",
      { signal: signal }
    );
  });
}, 'Aborting AIRwriter.rewrite().');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  const streamingResponse =
    rewriter.rewriteStreaming('hello', { signal: controller.signal });
  controller.abort();
  const reader = streamingResponse.getReader();
  await promise_rejects_dom(t, 'AbortError', reader.read());
}, 'Aborting AIRewriter.rewriteStreaming()');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  controller.abort();
  assert_throws_dom(
    'AbortError',
    () => rewriter.rewriteStreaming('hello', { signal: controller.signal }));
}, 'AIRewriter.rewriteStreaming() call with an aborted signal.');
