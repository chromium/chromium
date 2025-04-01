// META: script=resources/utils.js

promise_test(async () => {
  assert_true(!!Rewriter);
}, 'Rewriter must be defined.');

promise_test(async () => {
  // TODO(crbug.com/382615217): Test availability with various options.
  assert_equals(await Rewriter.availability(), 'available');
  assert_equals(await Rewriter.availability({ outputLanguage: 'en' }), 'available');
}, 'Rewriter.availability');

promise_test(async () => {
  const rewriter = await Rewriter.create();
  assert_equals(Object.prototype.toString.call(rewriter), '[object Rewriter]');
}, 'Rewriter.create() must be return a Rewriter.');

promise_test(async t => {
  let createResult = undefined;
  const progressEvents = [];
  let options = {};
  const downloadComplete = new Promise(resolve => {
    options.monitor = (m) => {
      m.addEventListener("downloadprogress", e => {
        assert_equals(createResult, undefined);
        assert_equals(e.total, 1);
        progressEvents.push(e);
        if (e.loaded == 1) { resolve(); }
      });
    };
  });

  createResult = await Rewriter.create(options);
  await downloadComplete;
  assert_greater_than_equal(progressEvents.length, 2);
  assert_equals(progressEvents.at(0).loaded, 0);
  assert_equals(progressEvents.at(-1).loaded, 1);
}, 'Rewriter.create() notifies its monitor on downloadprogress');

promise_test(async () => {
  const rewriter = await Rewriter.create();
  assert_equals(rewriter.sharedContext, '');
  assert_equals(rewriter.tone, 'as-is');
  assert_equals(rewriter.format, 'as-is');
  assert_equals(rewriter.length, 'as-is');
}, 'Rewriter.create() default values.');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const rewriter = await Rewriter.create({sharedContext: sharedContext});
  assert_equals(rewriter.sharedContext, sharedContext);
}, 'Rewriter.sharedContext');

promise_test(async () => {
  const rewriter = await Rewriter.create({tone: 'more-formal'});
  assert_equals(rewriter.tone, 'more-formal');
}, 'Creating a Rewriter with "more-formal" tone');

promise_test(async () => {
  const rewriter = await Rewriter.create({tone: 'more-casual'});
  assert_equals(rewriter.tone, 'more-casual');
}, 'Creating a Rewriter with "more-casual" tone');

promise_test(async () => {
  const rewriter = await Rewriter.create({format: 'plain-text'});
  assert_equals(rewriter.format, 'plain-text');
}, 'Creating a Rewriter with "plain-text" format');

promise_test(async () => {
  const rewriter = await Rewriter.create({format: 'markdown'});
  assert_equals(rewriter.format, 'markdown');
}, 'Creating a Rewriter with "markdown" format');

promise_test(async () => {
  const rewriter = await Rewriter.create({length: 'shorter'});
  assert_equals(rewriter.length, 'shorter');
}, 'Creating a Rewriter with "shorter" length');

promise_test(async () => {
  const rewriter = await Rewriter.create({length: 'longer'});
  assert_equals(rewriter.length, 'longer');
}, 'Creating a Rewriter with "longer" length');

promise_test(async () => {
  const rewriter = await Rewriter.create({
    expectedInputLanguages: ['en']
  });
  assert_array_equals(rewriter.expectedInputLanguages, ['en']);
}, 'Creating a Rewriter with expectedInputLanguages');

promise_test(async () => {
  const rewriter = await Rewriter.create({
    expectedContextLanguages: ['en']
  });
  assert_array_equals(rewriter.expectedContextLanguages, ['en']);
}, 'Creating a Rewriter with expectedContextLanguages');

promise_test(async () => {
  const rewriter = await Rewriter.create({
    outputLanguage: 'en'
  });
  assert_equals(rewriter.outputLanguage, 'en');
}, 'Creating a Rewriter with outputLanguage');

promise_test(async () => {
  const rewriter = await Rewriter.create({});
  assert_equals(rewriter.expectedInputLanguages, null);
  assert_equals(rewriter.expectedContextLanguages, null);
  assert_equals(rewriter.outputLanguage, null);
}, 'Creating a Rewriter without optional attributes');

promise_test(async (t) => {
  const rewriter = await Rewriter.create();
  let result = await rewriter.rewrite('');
  assert_equals(result, '');
  result = await rewriter.rewrite(' ');
  assert_equals(result, ' ');
}, 'Rewriter.rewrite() with an empty input or whitespace returns the ' +
    'original input');

promise_test(async (t) => {
  const rewriter = await Rewriter.create();
  const result = await rewriter.rewrite('hello', {context: ' '});
  assert_not_equals(result, '');
}, 'Rewriter.rewrite() with a whitespace context returns a non-empty result');

promise_test(async (t) => {
  const rewriter = await Rewriter.create();
  rewriter.destroy();
  await promise_rejects_dom(t, 'InvalidStateError', rewriter.rewrite('hello'));
}, 'Rewriter.rewrite() fails after destroyed');

promise_test(async (t) => {
  const rewriter = await Rewriter.create();
  rewriter.destroy();
  assert_throws_dom('InvalidStateError', () => rewriter.rewriteStreaming('hello'));
}, 'Rewriter.rewriteStreaming() fails after destroyed');

promise_test(async () => {
  const rewriter = await Rewriter.create();
  const result = await rewriter.measureInputUsage(kTestPrompt);
  assert_greater_than(result, 0);
}, 'Rewriter.measureInputUsage() returns non-empty result');
