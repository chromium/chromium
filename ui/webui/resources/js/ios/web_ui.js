// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window['chrome'] = window['chrome'] || {};

/**
 * Sends messages to the browser. See
 * https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md#chrome_send
 *
 * @param {string} message name to be passed to the browser.
 * @param {Array=} args optional.
 */
window['chrome']['send'] = function(message, args) {
  __gCrWeb.common.sendWebKitMessage('WebUIMessage', {
    'message': message,
    'arguments': args || [],
  });
};
