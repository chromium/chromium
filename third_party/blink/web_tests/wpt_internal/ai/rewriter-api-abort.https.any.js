// META: script=resources/utils.js

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return ai.rewriter.create({
      signal: signal
    });
  });
}, "Aborting AIRewriter.create().");

promise_test(async t => {
  const session = await ai.rewriter.create();
  await testAbortPromise(t, signal => {
    return session.rewrite(kTestPrompt, { signal: signal });
  });
}, "Aborting AIRewriter.rewrite().");

promise_test(async t => {
  const session = await ai.rewriter.create();
  await testAbortReadableStream(t, signal => {
    return session.rewriteStreaming(
      kTestPrompt, { signal: signal }
    );
  });
}, "Aborting AIRewriter.rewriteStreaming().");
