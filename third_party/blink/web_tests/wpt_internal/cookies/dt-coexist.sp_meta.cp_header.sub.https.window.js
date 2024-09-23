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
    // tools/origin_trials/generate_token.py https://web-platform.test:8444 DisableThirdPartyStoragePartitioning2 --expire-timestamp=2000000000
    spMeta.content = 'A2kO9VXomVHI2qXmDKzOIFbeBynCH0cFHdQsAJV2QjujjYKz5jHlNrcdGvgOrjZbRWHH/VUjmwuLOwBfraaFHg8AAAB2eyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiRGlzYWJsZVRoaXJkUGFydHlTdG9yYWdlUGFydGl0aW9uaW5nMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==';
    document.head.append(spMeta);

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
