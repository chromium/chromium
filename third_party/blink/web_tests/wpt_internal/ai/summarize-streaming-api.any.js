// META: script=resources/utils.js
// META: script=resources/workaround-for-362676838.js
// META: timeout=long

promise_test(async t => {
  const summarizer = await ai.summarizer.create();
  // Test the streaming summarize API.
  const streamingResponse = summarizer.summarizeStreaming(
    "The web-platform-tests Project is a cross-browser test suite for the Web-platform stack. Writing tests in a way that allows them to be run in all browsers gives browser projects confidence that they are shipping software that is compatible with other implementations, and that later implementations will be compatible with their implementations. This in turn gives Web authors/developers confidence that they can actually rely on the Web platform to deliver on the promise of working across browsers and devices without needing extra layers of abstraction to paper over the gaps left by specification editors and implementors.");
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
});
