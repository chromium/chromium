// META: script=resources/utils.js
// META: script=resources/workaround-for-382640509.js
// META: timeout=long

promise_test(async t => {
  await ensureLanguageModel();

  // Start a new session.
  const session = await ai.languageModel.create();
  // Test the streaming prompt API.
  const streamingResponse =
    session.promptStreaming(kTestPrompt);
  assert_equals(
    Object.prototype.toString.call(streamingResponse),
    "[object ReadableStream]"
  );
  const reader = streamingResponse.getReader();
  let result = "";
  while (true) {
    const { value, done } = await reader.read();
    if (done) {
      break;
    }
    if (value) {
      result += value;
    }
  }
  assert_greater_than(result.length, 0, "The result should not be empty.");
});
