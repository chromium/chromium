// META: script=resources/utils.js

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return Rewriter.create({ signal: signal });
  });
}, "Aborting Rewriter.create().");

promise_test(async t => {
  const session = await Rewriter.create();
  await testAbortPromise(t, signal => {
    return session.rewrite(kTestPrompt, { signal: signal });
  });
}, "Aborting Rewriter.rewrite().");

promise_test(async t => {
  const session = await Rewriter.create();
  await testAbortReadableStream(t, signal => {
    return session.rewriteStreaming(kTestPrompt, { signal: signal });
  });
}, "Aborting Rewriter.rewriteStreaming().");
