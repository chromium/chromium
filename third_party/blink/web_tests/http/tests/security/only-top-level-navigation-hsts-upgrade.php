<!DOCTYPE html>
<html>
<head>
    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script type="module">
  // Check HSTS upgrades only apply to top-level (outermost) frame navigations.
  // Note that this test must be run on an insecure origin because it relies on
  // insecure iframes being loadable. If it's instead run on a secure origin
  // then mixed content blocking will prevent HSTS from working.

  // 0) Confirm that insecure iframes can be loaded.
  // 1) Pin hsts-example.test to the HSTS via hsts.php
  // 2) Attempt to load an iframe via http. This should fail because
  //    http://hsts-example.test:8442 is an invalid origin *and* HSTS should not
  //    upgrade the iframe navigation to https.
  // 3) Open a new window and navigate it to the same http origin. This should
  //    successfully be upgraded to https, load, and then postMessage its origin.

  if (window.testRunner) {
    // This test matches the console output to an -expected.txt, so we'll
    // manually control when the test runner is finished.
    testRunner.waitUntilDone();
    testRunner.setPopupBlockingEnabled(false);
  }

  function onMessageWithTimeout() {
    return new Promise((resolve, reject) => {

    const timeoutID = setTimeout(() => {
      reject(new Error("Timeout: Didn't receive message"));
      onmessage = null;
    }, 3000);

    onmessage = (event) => {
      clearTimeout(timeoutID);
      resolve(event);
    };
  });
  };

  const iframeLoadable = document.createElement('iframe');
  const iframeLoadablePromise = onMessageWithTimeout()
  .then((event) => console.log("iframe successfully loaded via http"));
  // Step 0.
  iframeLoadable.src = "http://hsts-example.test:8000/security/resources/hsts.php";
  document.body.appendChild(iframeLoadable);
  await iframeLoadablePromise;

  // Step 1.
  // Add HSTS pin for domain.
  await fetch("https://hsts-example.test:8443/security/resources/hsts.php?as-fetch");

  // Note: HTTP, not HTTPS:
  const hstsIframe = document.createElement('iframe');
  const hstsIframePromise = onMessageWithTimeout()
  .then((event) => console.log("HSTS iframe unexpectedly loaded"))
  .catch((error) => console.log("HSTS iframe successfully didn't load"));
  // Step 2.
  hstsIframe.src = "http://hsts-example.test:8443/security/resources/hsts.php";
  document.body.appendChild(hstsIframe);
  await hstsIframePromise;


  const hstsWindowPromise = onMessageWithTimeout()
  .then((event) => console.log("Window's origin is: " + event.data.origin));
  // Step 3.
  const w = window.open("http://hsts-example.test:8443/security/resources/post-origin-to-opener.html", "_blank");
  if(!w) {
    console.log("Window didn't open. Is there a popup blocker?");
  }

  await hstsWindowPromise;

  if (window.testRunner) {
  testRunner.notifyDone();
  }

</script>
</body>
</html>