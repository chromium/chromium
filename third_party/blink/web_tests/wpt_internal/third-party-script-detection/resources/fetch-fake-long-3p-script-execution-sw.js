// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', (event) => {
    // Specify the URLs of scripts you want to replace
    const targetScriptUrls = [
        // Word Press
        'https://c0.wp.com/c/6.4.2/wp-includes/js/dist/vendor/wp-polyfill.min.js',
        // Google Analytics
        'https://www.google-analytics.com/analytics.js',
        // Google Font Api
        'https://www.googleapis.com/example/webfont'
    ];

    // Check if the request is for one of the target 3p scripts
    if (targetScriptUrls.includes(event.request.url)) {
        // Fake long script
        const customScript = `
        const before = performance.now();
        const long_script_duration = 50;
        while (performance.now() < (before + long_script_duration)) { }
      `;

        // send a response with the custom long script
        event.respondWith(
            new Response(customScript, {
                headers: { 'Content-Type': 'text/javascript' }
            })
        );
    }
});
