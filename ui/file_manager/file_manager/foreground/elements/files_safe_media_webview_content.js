// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = () => {
  const FILES_APP_ORIGIN =
      'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';
  let messageSource;

  const content = document.querySelector('#content');

  window.addEventListener('message', event => {
    if (event.origin !== FILES_APP_ORIGIN) {
      console.error('Unknown origin: ' + event.origin);
      return;
    }

    messageSource = event.source;
    switch (event.data.type) {
      case 'html':
        content.textContent = '';
        contentChanged(null);
        fetch(event.data.src)
            .then((response) => {
              return response.text();
            })
            .then((text) => {
              content.textContent = text;
              contentChanged(text);
            });
        break;
      case 'audio':
      case 'video':
        content.onloadeddata = (e) => contentChanged(e.target.src);
        content.src = event.data.src;
        break;
      case 'image':
        content.remove();
        content.onload = (e) => contentChanged(e.target.src);
        content.src = event.data.src;
        content.decode()
            .then(() => {
              content.removeAttribute('generic-thumbnail');
              document.body.appendChild(content);
            })
            .catch(() => {
              content.setAttribute('generic-thumbnail', 'image');
              document.body.appendChild(content);
            });
        break;
      default:
        content.onload = (e) => contentChanged(e.target.src);
        content.src = event.data.src;
        break;
    }
  });

  document.addEventListener('contextmenu', e => {
    e.preventDefault();
    return false;
  });

  document.addEventListener('click', e => {
    sendMessage((e.target === content) ? 'tap-inside' : 'tap-outside');
  });

  function contentChanged(src) {
    sendMessage(src ? 'webview-loaded' : 'webview-cleared');
  }

  function sendMessage(message) {
    if (messageSource) {
      messageSource.postMessage(message, FILES_APP_ORIGIN);
    }
  }

  // TODO(oka): This is a workaround to fix FOUC problem, where sometimes
  // immature view with smaller window size than outer window is rendered for a
  // moment. Remove this after the root cause is fixed. http://crbug.com/640525
  window.addEventListener('resize', () => {
    // Remove hidden attribute on event of resize to avoid FOUC. The window's
    // initial size is 100 x 100 and it's fit into the outer window size after a
    // moment. Due to Files App's window size constraint, resized window must be
    // larger than 100 x 100. So this event is always invoked.
    content.removeAttribute('hidden');
  });
  // Fallback for the case of webview bug is fixed and above code is not
  // executed.
  setTimeout(() => {
    content.removeAttribute('hidden');
  }, 500);
};
