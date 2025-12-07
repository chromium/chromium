// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window['chrome'] = window['chrome'] || {};

/**
 * Sends messages to the browser. See
 * https://chromium.googlesource.com/chromium/src/+/main/docs/webui/webui_explainer.md#chrome_send
 *
 * @param {string} message name to be passed to the browser.
 * @param {Array=} args optional.
 */
window['chrome']['send'] = function(message, args) {
  try {
    // A web page can override `window.webkit` with any value. Deleting the
    // object ensures that original and working implementation of
    // window.webkit is restored.
    const oldWebkit = window.webkit;
    delete window['webkit'];
    window.webkit.messageHandlers['WebUIMessage'].postMessage(
        {'message': message, 'arguments': args || []});
    window.webkit = oldWebkit;
  } catch (err) {
    // TODO(crbug.com/40269960): Report this fatal error
  }
};
