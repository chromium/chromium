promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  iframe.contentWindow.AIRewriter.create();
  iframe.remove();
}, 'Detaching iframe during AIRewriter.create() should not leak memory');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const iframeRewriter = iframeWindow.AIRewriter;
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, iframeRewriter.create());
}, 'AIRewriter.create() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeDOMException = iframe.contentWindow.DOMException;
  const rewriter = await iframe.contentWindow.AIRewriter.create();
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, rewriter.rewrite('hello'));
}, 'AIRewriter.rewrite() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const rewriter = await iframeWindow.AIRewriter.create();
  iframe.remove();

  assert_throws_dom(
      'InvalidStateError', iframeDOMException, () => rewriter.rewriteStreaming('hello'));
}, 'AIRewriter.rewriteStreaming() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const rewriter = await iframe.contentWindow.AIRewriter.create();
  rewriter.rewrite('hello');
  iframe.remove();
}, 'Detaching iframe during AIRewriter.rewrite() should not leak memory');
