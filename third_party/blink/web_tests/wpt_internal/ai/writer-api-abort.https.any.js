// META: script=resources/utils.js

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return ai.writer.create({
      signal: signal
    });
  });
}, "Aborting AIWriter.create().");

promise_test(async t => {
  const session = await ai.writer.create();
  await testAbortPromise(t, signal => {
    return session.write(kTestPrompt, { signal: signal });
  });
}, "Aborting AIWriter.write().");

promise_test(async t => {
  const session = await ai.writer.create();
  await testAbortReadableStream(t, signal => {
    return session.writeStreaming(
      kTestPrompt, { signal: signal }
    );
  });
}, "Aborting AIWriter.writeStreaming().");
