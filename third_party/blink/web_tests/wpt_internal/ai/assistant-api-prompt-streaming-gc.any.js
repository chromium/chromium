// META: script=resources/workaround-for-362676838.js
// META: timeout=long

promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.assistant.capabilities();
  const status = capabilities.available;
  assert_true(status === 'readily');
  // Start a new session.
  const session = await ai.assistant.create();
  // Test the streaming prompt API.
  const streamingResponse = session.promptStreaming("What is 1+2?");
  // Run GC.
  gc();
  assert_true(Object.prototype.toString.call(streamingResponse) === "[object ReadableStream]");
  const reader = streamingResponse.getReader();
  let result = "";
  while (true) {
    const { value, done } = await reader.read();
    if (done) {
      break;
    }
    result = value;
  }
  assert_true(result.length > 0);
}, 'Prompt Streaming API must continue even after GC has been performed.');
