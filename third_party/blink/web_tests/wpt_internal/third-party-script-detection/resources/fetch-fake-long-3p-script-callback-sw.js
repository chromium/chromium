// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
importScripts('third-party-script-test-utils.js');

self.addEventListener('fetch', (event) => {
    // Check if the request is for one of the target 3p scripts
    if (targetScriptUrls.includes(event.request.url)) {
        // Fake long callback
        const customScript = `
        document.querySelector("#third-party-script-element-id").addEventListener('load', () => {
            const before = performance.now();
            const long_script_duration = 50;
            while (performance.now() < (before + long_script_duration)) { }
        });
      `;

        // send a response with the custom long script
        event.respondWith(
            new Response(customScript, {
                headers: { 'Content-Type': 'text/javascript' }
            })
        );
    }
});
