// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load the ShadyDOM polyfill only if needed, on first web component definition.
(function () {
  function loadScripts(webcomponentsBundleContent) {
    var polyfillScript = document.createElement("script");
    // We need to synchronously execute the polyfill if required. To facilitate
    // this we copy the polyfill contents into the script.
    polyfillScript.innerHTML = `
    if (window.customElements) {
      const ceDefine = customElements.define;
      customElements.define = function() {
        customElements.define = ceDefine;
        if (!window.ShadyDOM) {
          window.ShadyDOM = {force: true};
          (function(){
            ${webcomponentsBundleContent}
          })();
          // Ensure a late loaded polyfill can no longer be applied.
          Object.defineProperty(window, 'ShadyDOM', {value: window.ShadyDOM,
            configurable: false, writable: false});
        }
        customElements.define.apply(customElements, arguments);
      }
    }
    `;
    document.documentElement.appendChild(polyfillScript);
  }

  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (xhr.readyState === XMLHttpRequest.DONE) {
      loadScripts(xhr.responseText);
    }
  }
  xhr.open('GET', chrome.extension.getURL("/webcomponents-bundle.js"));
  xhr.send();
})();
