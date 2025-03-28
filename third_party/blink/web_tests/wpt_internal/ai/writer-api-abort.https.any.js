// META: script=resources/utils.js

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return Writer.create({ signal: signal });
  });
}, "Aborting Writer.create().");

promise_test(async t => {
  const session = await Writer.create();
  await testAbortPromise(t, signal => {
    return session.write(kTestPrompt, { signal: signal });
  });
}, "Aborting Writer.write().");

promise_test(async t => {
  const session = await Writer.create();
  await testAbortReadableStream(t, signal => {
    return session.writeStreaming(kTestPrompt, { signal: signal });
  });
}, "Aborting Writer.writeStreaming().");
