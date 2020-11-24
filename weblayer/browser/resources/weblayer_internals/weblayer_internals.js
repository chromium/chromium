// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Javascript module for chrome://weblayer. */

import {isAndroid} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {PageHandler} from './weblayer_internals.mojom-webui.js';

/* Main entry point. */
window.document.addEventListener('DOMContentLoaded', async function() {
  // Setup backend mojo.
  const pageHandler = PageHandler.getRemote();
  if (isAndroid) {
    const {enabled} = await pageHandler.getRemoteDebuggingEnabled();
    const checkbox = $('remote-debug');
    checkbox.checked = enabled;
    checkbox.addEventListener('click', (event) => {
      pageHandler.setRemoteDebuggingEnabled(event.target.checked);
    });

    $('remote-debug-label').removeAttribute('hidden');
  }
});
