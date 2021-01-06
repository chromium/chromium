// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load the Shady DOM polyfill as soon as possible.
(function() {
  function loadScripts() {
    if (!document.head) {
      setTimeout(loadScripts, 0);
      return;
    }
    var script1 = document.createElement('script');
    script1.innerHTML = `
      window.ShadyDOM = {force: true, noPatch: true};
    `;
    var script2 = document.createElement('script');
    script2.src = chrome.extension.getURL('/shadydom.js');
    document.head.prepend(script1);
    document.head.prepend(script2);
  }
  loadScripts();
}());
