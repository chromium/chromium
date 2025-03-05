// META: script=resources/utils.js
// META: timeout=long

promise_test(async () => {
  const availability = await ai.summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
  });
  assert_not_equals(availability, "unavailable");
}, 'AISummarizerFactory.availability is available');

promise_test(async () => {
  const availability = await ai.summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
    expectedInputLanguages: ["en-GB"],
    expectedContextLanguages: ["en"],
    outputLanguage: "en",
  });
  assert_not_equals(availability, "unavailable");
}, 'AISummarizerFactory.availability is available for supported languages');

promise_test(async () => {
  const availability = await ai.summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
    expectedInputLanguages: ["es"], // not supported
    expectedContextLanguages: ["en"],
    outputLanguage: "es", // not supported
  });
  assert_equals(availability, "unavailable");
}, 'AISummarizerFactory.availability returns no for unsupported languages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({});
  const response = await summarizer.summarize(
    "The web-platform-tests Project is a cross-browser test suite for the Web-platform stack. Writing tests in a way that allows them to be run in all browsers gives browser projects confidence that they are shipping software that is compatible with other implementations, and that later implementations will be compatible with their implementations. This in turn gives Web authors/developers confidence that they can actually rely on the Web platform to deliver on the promise of working across browsers and devices without needing extra layers of abstraction to paper over the gaps left by specification editors and implementors.");
  assert_equals(typeof response, "string");
  assert_greater_than(response.length, 0);
});

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const summarizer = await createSummarizerMaybeDownload({sharedContext: sharedContext});
  assert_equals(summarizer.sharedContext, sharedContext);
}, 'AISummarizer.sharedContext');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({type: 'headline'});
  assert_equals(summarizer.type, 'headline');
}, 'AISummarizer.type');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({format: 'markdown'});
  assert_equals(summarizer.format, 'markdown');
}, 'AISummarizer.format');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({length: 'medium'});
  assert_equals(summarizer.length, 'medium');
}, 'AISummarizer.length');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    expectedInputLanguages: ['en']
  });
  assert_array_equals(summarizer.expectedInputLanguages, ['en']);
}, 'AISummarizer.expectedInputLanguages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    expectedContextLanguages: ['en']
  });
  assert_array_equals(summarizer.expectedContextLanguages, ['en']);
}, 'AISummarizer.expectedContextLanguages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    outputLanguage: 'en'
  });
  assert_equals(summarizer.outputLanguage, 'en');
}, 'AISummarizer.outputLanguage');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({});
  assert_equals(summarizer.expectedInputLanguages, null);
  assert_equals(summarizer.expectedContextLanguages, null);
  assert_equals(summarizer.outputLanguage, null);
}, 'AISummarizer optional attributes return null');
