async function runTestInPrerender(testName) {
  const prerender = "session-history-prerender.https.html";
  const testChannel = new BroadcastChannel(`test-channel-${testName}`);
  const result = new Promise((resolve) => {
    testChannel.addEventListener(
      "message",
      (e) => {
        testChannel.close();
        resolve(e.data);
      },
      { once: true },
    );
  });

  // Run test in a new window for test isolation.
  window.open(
    `./resources/session-history-initiator.https.html?prerender=${prerender}&testName=${testName}`,
    "_blank",
    "noopener",
  );
  return result;
}

// This will activate the prerendered context created in runTestInPrerender
// and then run the post-activation variation of |testName|.
async function runTestInActivatedPage(testName) {
  const testChannel = new BroadcastChannel(`test-channel-${testName}`);

  const result = new Promise((resolve) => {
    testChannel.addEventListener(
      "message",
      (e) => {
        testChannel.close();
        resolve(e.data);
      },
      { once: true },
    );
  });

  testChannel.postMessage("activate");
  return result;
}
