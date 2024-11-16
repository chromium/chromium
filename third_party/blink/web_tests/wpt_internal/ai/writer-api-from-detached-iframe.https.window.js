promise_test(
    async (t) => {
      // Create the iframe and append it to the document.
      const iframe = document.createElement('iframe');
      document.childNodes[document.childNodes.length - 1].appendChild(iframe);
      iframe.contentWindow.ai.writer.create();
      // Detach the iframe.
      iframe.remove();
    },
    'Detaching iframe while runing AIWriterFactory.create() should not cause ' +
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
      iframeWindow.ai.writer.create());
}, 'AIWriterFactory.create() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const iframeDOMException = iframe.contentWindow.DOMException;

  const writer = await iframe.contentWindow.ai.writer.create();

  // Detach the iframe.
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, writer.write('hello'));
}, 'AIWriter.write() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;

  const writer = await iframeWindow.ai.writer.create();

  // Detach the iframe.
  iframe.remove();

  assert_throws_dom(
      'InvalidStateError', iframeDOMException,
      () => writer.writeStreaming('hello'));
}, 'AIWriter.writeStreaming() fails on a detached iframe.');

promise_test(async (t) => {
  // Create the iframe and append it to the document.
  const iframe = document.createElement('iframe');
  document.childNodes[document.childNodes.length - 1].appendChild(iframe);
  const writer = await iframe.contentWindow.ai.writer.create();
  writer.write('hello');
  // Detach the iframe.
  iframe.remove();
}, 'Detaching iframe while runing AIWriter.write() should not cause memory leak');
