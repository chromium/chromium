// META: script=resources/utils.js

promise_test(async () => {
  const summarizer = await ai.summarizer.create();
  const response = await summarizer.summarize(
    "The web-platform-tests Project is a cross-browser test suite for the Web-platform stack. Writing tests in a way that allows them to be run in all browsers gives browser projects confidence that they are shipping software that is compatible with other implementations, and that later implementations will be compatible with their implementations. This in turn gives Web authors/developers confidence that they can actually rely on the Web platform to deliver on the promise of working across browsers and devices without needing extra layers of abstraction to paper over the gaps left by specification editors and implementors.");
  assert_true(typeof response === "string");
  assert_true(response.length > 0);
});
