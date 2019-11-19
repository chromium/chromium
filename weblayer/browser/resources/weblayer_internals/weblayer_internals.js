// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/* Javascript module for chrome://weblayer. */
(function() {

/* Main entry point. */
document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  const pageHandler = weblayerInternals.mojom.PageHandler.getRemote();
  if (cr.isAndroid) {
    pageHandler.getRemoteDebuggingEnabled().then((response) => {
        let checkbox = $('remote-debug');
        checkbox.checked = response.enabled;
        checkbox.addEventListener('click', (event) => {
          pageHandler.setRemoteDebuggingEnabled(event.target.checked);
        });

        $('remote-debug-label').removeAttribute('hidden');
    });
  }
});

}());
