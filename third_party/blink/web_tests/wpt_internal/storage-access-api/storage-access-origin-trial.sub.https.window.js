// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

'use strict';

// Here's the set-up for this test:
// Step 1 (top-frame) Set up listener for "HasAccess" message.
// Step 2 (top-frame) Embed an iframe that's cross-site with top-frame.
// Step 3 (sub-frame) Try to use storage access API.
// Step 4 (sub-frame) Send "HasAccess" message to top-frame.
// Step 5 (top-frame) Receive "HasAccess" message and cleanup.

async_test(t => {
  // Step 1
  window.addEventListener("message", t.step_func(e => {
    // Step 5
    assert_equals(e.data, "HasAccess", "Storage Access API should be accessable");
    t.done();
  }));

  // Step 2
  let iframe = document.createElement("iframe");
  iframe.src = "https://{{hosts[alt][]}}:{{ports[https][0]}}/wpt_internal/storage-access-api/resources/storage-access-origin-trial-iframe.html";
  document.body.appendChild(iframe);
}, "Verify StorageAccessAPIBeyondCookies OT works");

