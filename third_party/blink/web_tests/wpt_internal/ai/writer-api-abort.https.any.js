// META: script=resources/workaround-for-362676838.js
// META: script=resources/utils.js

promise_test(async (t) => {
  testAbort(t, (signal) => {
    return ai.writer.create({
      signal: signal
    });
  });
}, 'Aborting AIWriter.create().');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  testAbort(t, (signal) => {
    return writer.write(
      "Write a poem",
      { signal: signal }
    );
  });
}, 'Aborting AIWriter.writer().');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  const controller = new AbortController();
  const streamingResponse =
    writer.writeStreaming('hello', { signal: controller.signal });
  controller.abort();
  const reader = streamingResponse.getReader();
  await promise_rejects_dom(t, 'AbortError', reader.read());
}, 'Aborting AIWriter.writeStreaming()');

promise_test(async (t) => {
  const writer = await ai.writer.create();
  const controller = new AbortController();
  controller.abort();
  assert_throws_dom(
    'AbortError',
    () => writer.writeStreaming('hello', { signal: controller.signal }));
}, 'AIWriter.writeStreaming() call with an aborted signal.');
