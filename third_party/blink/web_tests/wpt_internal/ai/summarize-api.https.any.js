// META: script=resources/utils.js
// META: timeout=long

promise_test(async () => {
  assert_true(!!Summarizer);
}, 'Summarizer must be defined.');

promise_test(async () => {
  const availability = await Summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
  });
  assert_not_equals(availability, "unavailable");
}, 'Summarizer.availability() is available');

promise_test(async () => {
  const availability = await Summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
    expectedInputLanguages: ["en-GB"],
    expectedContextLanguages: ["en"],
    outputLanguage: "en",
  });
  assert_not_equals(availability, "unavailable");
}, 'Summarizer.availability() is available for supported languages');

promise_test(async () => {
  const availability = await Summarizer.availability({
    type: "tl;dr",
    format: "plain-text",
    length: "medium",
    expectedInputLanguages: ["es"], // not supported
    expectedContextLanguages: ["en"],
    outputLanguage: "es", // not supported
  });
  assert_equals(availability, "unavailable");
}, 'Summarizer.availability() returns no for unsupported languages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({});
  const result = await summarizer.summarize(kTestPrompt);
  assert_equals(typeof result, "string");
  assert_greater_than(result.length, 0);
}, 'Summarizer.summarize() returns non-empty result');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({});
  const result = await summarizer.measureInputUsage(kTestPrompt);
  assert_greater_than(result, 0);
}, 'Summarizer.measureInputUsage() returns non-empty result');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const summarizer = await createSummarizerMaybeDownload({sharedContext: sharedContext});
  assert_equals(summarizer.sharedContext, sharedContext);
}, 'Summarizer.sharedContext');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({type: 'headline'});
  assert_equals(summarizer.type, 'headline');
}, 'Summarizer.type');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({format: 'markdown'});
  assert_equals(summarizer.format, 'markdown');
}, 'Summarizer.format');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({length: 'medium'});
  assert_equals(summarizer.length, 'medium');
}, 'Summarizer.length');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    expectedInputLanguages: ['en']
  });
  assert_array_equals(summarizer.expectedInputLanguages, ['en']);
}, 'Summarizer.expectedInputLanguages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    expectedContextLanguages: ['en']
  });
  assert_array_equals(summarizer.expectedContextLanguages, ['en']);
}, 'Summarizer.expectedContextLanguages');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({
    outputLanguage: 'en'
  });
  assert_equals(summarizer.outputLanguage, 'en');
}, 'Summarizer.outputLanguage');

promise_test(async () => {
  const summarizer = await createSummarizerMaybeDownload({});
  assert_equals(summarizer.expectedInputLanguages, null);
  assert_equals(summarizer.expectedContextLanguages, null);
  assert_equals(summarizer.outputLanguage, null);
}, 'Summarizer optional attributes return null');