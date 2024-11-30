// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

'use strict';

// Here's the set-up for this test:
// Step 1 (window) Set up listener for "SetData" and "HadData" message.
// Step 2 (window) Open other window on cross-site origin.
// Step 3 (other-window) Set cookies and local storage.
// Step 4 (other-window) Send "SetData" message.
// Step 5 (window) Embed iframe on cross-site origin.
// Step 6 (iframe) Check for cookies and local storage.
// Step 7 (iframe) Send "HadData" message.
// Step 8 (window) Cleanup.

async_test(t => {
    const spMeta = document.createElement('meta');
    spMeta.httpEquiv = 'origin-trial';
    // The token below was generated via the following command:
    // tools/origin_trials/generate_token.py https://web-platform.test:8444 DisableThirdPartyStoragePartitioning3 --expire-timestamp=2000000000
    spMeta.content = 'A8Kgrgshi3gcxgc7GuTO8U6nTltb9zaaNVymhE6MAfo7EEoJWBfhvD5NNCjiu+nIHNRTSL97Mq7xg+VKkOPbQAkAAAB2eyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiRGlzYWJsZVRoaXJkUGFydHlTdG9yYWdlUGFydGl0aW9uaW5nMyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==';
    document.head.append(spMeta);
    // The token in the header was generated via the following command:
    // tools/origin_trials/generate_token.py https://web-platform.test:8444 TopLevelTpcd --expire-timestamp=2000000000

    // Step 1
    window.addEventListener("message", t.step_func(e => {
        if (e.data.type == "SetData") {
            // Step 5
            let iframe = document.createElement("iframe");
            iframe.src = "https://{{hosts[alt][]}}:{{ports[https][0]}}/wpt_internal/cookies/resources/iframe.html";
            document.body.appendChild(iframe);
        } else if (e.data.type == "HadData") {
            // Step 8
            assert_equals(e.data.message, "Cookie,LocalStorage,", "Unpartitioned data should be accessable");
            t.done();
        }
    }));

    // Step 2
    window.open("https://{{hosts[alt][]}}:{{ports[https][0]}}/wpt_internal/cookies/resources/window.html");
}, "Verify origin SP Meta + CP Header Deprecation Trials can coexist");
