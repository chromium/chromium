'use strict';

test(() => {
  let iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  const mediaDevices = iframe.contentWindow.navigator.mediaDevices;
  const iframeDOMException = iframe.contentWindow.DOMException;
  document.body.removeChild(iframe);
  iframe = null;
  GCController.collect();

  assert_throws_dom('InvalidStateError', iframeDOMException, () => {
    mediaDevices.setCaptureHandleConfig();
  });
}, 'setCaptureHandleConfig raises InvalidStateError in a detached context.');
