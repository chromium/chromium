promise_test(
    async (t) => {
      // Create the iframe and append it to the document.
      const iframe = document.createElement('iframe');
      document.childNodes[document.childNodes.length - 1].appendChild(iframe);
      iframe.contentWindow.ai.rewriter.create();
      // Detach the iframe.
      iframe.remove();
    },
    'Detaching iframe while runing AIRewriterFactory.create() should not cause ' +
        'memory leak');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;

  // Detach the iframe.
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException,
      iframeWindow.ai.rewriter.create());
}, 'AIRewriterFactory.create() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const iframeDOMException = iframe.contentWindow.DOMException;

  const rewriter = await iframe.contentWindow.ai.rewriter.create();

  // Detach the iframe.
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, rewriter.rewrite('hello'));
}, 'AIRewriter.rewrite() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;

  const rewriter = await iframeWindow.ai.rewriter.create();

  // Detach the iframe.
  iframe.remove();

  assert_throws_dom(
      'InvalidStateError', iframeDOMException,
      () => rewriter.rewriteStreaming('hello'));
}, 'AIRewriter.rewriteStreaming() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const rewriter = await iframe.contentWindow.ai.rewriter.create();
  rewriter.rewrite('hello');
  // Detach the iframe.
  iframe.remove();
}, 'Detaching iframe while runing AIRewriter.rewrite() should not cause memory leak');
