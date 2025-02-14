promise_test(async () => {
  assert_true(!!ai);
  assert_true(!!ai.rewriter);
  assert_equals(
      Object.prototype.toString.call(ai.rewriter), '[object AIRewriterFactory]');
}, 'AIRewriterFactory must be available.');

promise_test(async () => {
  // TODO(crbug.com/382615217): Test availability with various options.
  assert_equals(await ai.rewriter.availability(), 'available');
  assert_equals(await ai.rewriter.availability({ outputLanguage: 'en' }), 'available');
}, 'AIRewriterFactory.availability');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  assert_equals(Object.prototype.toString.call(rewriter), '[object AIRewriter]');
}, 'AIRewriterFactory.create() must be return a AIRewriter.');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  assert_equals(rewriter.sharedContext, '');
  assert_equals(rewriter.tone, 'as-is');
  assert_equals(rewriter.format, 'as-is');
  assert_equals(rewriter.length, 'as-is');
}, 'AIRewriterFactory.create() default values.');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const rewriter = await ai.rewriter.create({sharedContext: sharedContext});
  assert_equals(rewriter.sharedContext, sharedContext);
}, 'AIRewriter.sharedContext');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({tone: 'more-formal'});
  assert_equals(rewriter.tone, 'more-formal');
}, 'Creating a AIRewriter with "more-formal" tone');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({tone: 'more-casual'});
  assert_equals(rewriter.tone, 'more-casual');
}, 'Creating a AIRewriter with "more-casual" tone');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({format: 'plain-text'});
  assert_equals(rewriter.format, 'plain-text');
}, 'Creating a AIRewriter with "plain-text" format');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({format: 'markdown'});
  assert_equals(rewriter.format, 'markdown');
}, 'Creating a AIRewriter with "markdown" format');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({length: 'shorter'});
  assert_equals(rewriter.length, 'shorter');
}, 'Creating a AIRewriter with "shorter" length');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({length: 'longer'});
  assert_equals(rewriter.length, 'longer');
}, 'Creating a AIRewriter with "longer" length');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({
    expectedInputLanguages: ['en']
  });
  assert_array_equals(rewriter.expectedInputLanguages, ['en']);
}, 'Creating a AIRewriter with expectedInputLanguages');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({
    expectedContextLanguages: ['en']
  });
  assert_array_equals(rewriter.expectedContextLanguages, ['en']);
}, 'Creating a AIRewriter with expectedContextLanguages');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({
    outputLanguage: 'en'
  });
  assert_equals(rewriter.outputLanguage, 'en');
}, 'Creating a AIRewriter with outputLanguage');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({});
  assert_equals(rewriter.expectedInputLanguages, null);
  assert_equals(rewriter.expectedContextLanguages, null);
  assert_equals(rewriter.outputLanguage, null);
}, 'Creating a AIRewriter without optional attributes');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  let result = await rewriter.rewrite('');
  assert_equals(result, '');
  result = await rewriter.rewrite(' ');
  assert_equals(result, ' ');
}, 'AIRewriter.rewrite() with an empty input or whitespace returns the ' +
    'original input');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const result = await rewriter.rewrite('hello', {context: ' '});
  assert_not_equals(result, '');
}, 'AIRewriter.rewrite() with a whitespace context returns a non-empty result');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  rewriter.destroy();
  await promise_rejects_dom(t, 'InvalidStateError', rewriter.rewrite('hello'));
}, 'AIRewriter.rewrite() fails after destroyed');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  rewriter.destroy();
  assert_throws_dom('InvalidStateError', () => rewriter.rewriteStreaming('hello'));
}, 'AIRewriter.rewriteStreaming() fails after destroyed');
