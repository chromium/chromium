// META: script=resources/utils.js

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return AISummarizer.create({ signal: signal });
  });
}, "Aborting AISummarizer.create().");

promise_test(async t => {
  const session = await AISummarizer.create();
  await testAbortPromise(t, signal => {
    return session.summarize(kTestPrompt, { signal: signal });
  });
}, "Aborting AISummarizer.summarize().");

promise_test(async t => {
  const session = await AISummarizer.create();
  await testAbortReadableStream(t, signal => {
    return session.summarizeStreaming(kTestPrompt, { signal: signal });
  });
}, "Aborting AISummarizer.summarizeStreaming().");
