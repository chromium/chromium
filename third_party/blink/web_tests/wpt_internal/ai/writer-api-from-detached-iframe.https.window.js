promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  iframe.contentWindow.AIWriter.create();
  iframe.remove();
}, 'Detaching iframe during AIWriter.create() should not leak memory');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const iframeWriter = iframeWindow.AIWriter;
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, iframeWriter.create());
}, 'AIWriter.create() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeDOMException = iframe.contentWindow.DOMException;
  const writer = await iframe.contentWindow.AIWriter.create();
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, writer.write('hello'));
}, 'AIWriter.write() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const writer = await iframeWindow.AIWriter.create();
  iframe.remove();

  assert_throws_dom(
      'InvalidStateError', iframeDOMException, () => writer.writeStreaming('hello'));
}, 'AIWriter.writeStreaming() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const writer = await iframe.contentWindow.AIWriter.create();
  writer.write('hello');
  iframe.remove();
}, 'Detaching iframe during AIWriter.write() should not leak memory');
