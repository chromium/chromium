// See crbug.com/1307461 for more details.
test(() => {
  let iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  let context = new iframe.contentWindow.AudioContext();
  let iframeDOMException = iframe.contentWindow.DOMException;
  document.body.removeChild(iframe);
  iframe = null;
  GCController.collect();

  assert_throws_dom('InvalidStateError', iframeDOMException, () => {
    context.destination.channelCount = 1;
  });
}, 'Changing channel after detaching the document throws an exception.');
