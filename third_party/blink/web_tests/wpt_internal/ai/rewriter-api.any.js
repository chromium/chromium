promise_test(async () => {
  assert_true(!!ai);
  assert_true(!!ai.rewriter);
  assert_equals(
      Object.prototype.toString.call(ai.rewriter), '[object AIRewriterFactory]');
}, 'AIRewriterFactory must be available.');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  assert_equals(Object.prototype.toString.call(rewriter), '[object AIRewriter]');
}, 'AIRewriterFactory.create() must be return a AIRewriter.');

promise_test(async (t) => {
  const controller = new AbortController();
  const createPromise = ai.rewriter.create({signal: controller.signal});
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', createPromise);
}, 'Aborting AIRewriterFactory.create().');

promise_test(async (t) => {
  const controller = new AbortController();
  controller.abort();
  const createPromise = ai.rewriter.create({signal: controller.signal});
  await promise_rejects_dom(t, 'AbortError', createPromise);
}, 'AIRewriterFactory.create() call with an aborted signal.');

promise_test(async () => {
  const sharedContext = 'This is a shared context string';
  const rewriter = await ai.rewriter.create({sharedContext: sharedContext});
  assert_equals(rewriter.sharedContext, sharedContext);
}, 'AIRewriter.sharedContext');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  assert_equals(rewriter.tone, 'as-is');
}, 'AIRewriter.tone default value');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({tone: 'more-formal'});
  assert_equals(rewriter.tone, 'more-formal');
}, 'Creating a AIRewriter with "more-formal" tone');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({tone: 'more-casual'});
  assert_equals(rewriter.tone, 'more-casual');
}, 'Creating a AIRewriter with "more-casual" tone');

promise_test(async () => {
  const rewriter = await ai.rewriter.create();
  assert_equals(rewriter.length, 'as-is');
}, 'AIRewriter.length default value');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({length: 'shorter'});
  assert_equals(rewriter.length, 'shorter');
}, 'Creating a AIRewriter with "shorter" length');

promise_test(async () => {
  const rewriter = await ai.rewriter.create({length: 'longer'});
  assert_equals(rewriter.length, 'longer');
}, 'Creating a AIRewriter with "longer" length');

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

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  const writePromise = rewriter.rewrite('hello', {signal: controller.signal});
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', writePromise);
}, 'Aborting AIRewriter.rewrite().');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  controller.abort();
  const writePromise = rewriter.rewrite('hello', {signal: controller.signal});
  await promise_rejects_dom(t, 'AbortError', writePromise);
}, 'AIRewriter.rewrite() call with an aborted signal.');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  const streamingResponse =
      rewriter.rewriteStreaming('hello', {signal: controller.signal});
  controller.abort();
  const reader = streamingResponse.getReader();
  await promise_rejects_dom(t, 'AbortError', reader.read());
}, 'Aborting AIRewriter.rewriteStreaming()');

promise_test(async (t) => {
  const rewriter = await ai.rewriter.create();
  const controller = new AbortController();
  controller.abort();
  assert_throws_dom(
      'AbortError',
      () => rewriter.rewriteStreaming('hello', {signal: controller.signal}));
}, 'AIRewriter.rewriteStreaming() call with an aborted signal.');
