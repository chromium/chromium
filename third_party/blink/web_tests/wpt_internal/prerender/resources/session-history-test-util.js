// Note: Following utility functions are expected to be used from
// session-history-* test files.

async function waitChannelMessage(testName) {
  const result = new Promise((resolve) => {
    const testChannel = new BroadcastChannel(testName);
    testChannel.addEventListener(
      "message",
      (e) => {
        testChannel.close();
        resolve(e.data);
      },
      { once: true },
    );
  });
  return result;
}

async function runTestInPrerender(testName) {
  const result = waitChannelMessage(`test-channel-${testName}`);

  // Run test in a new window for test isolation.
  const prerender = "session-history-prerender.https.html";
  window.open(
    `./resources/session-history-initiator.https.html?prerender=${prerender}&testName=${testName}`,
    "_blank",
    "noopener",
  );
  return result;
}

// This will activate the prerendered context created in runTestInPrerender
// and then run the post-activation variation of `testName`.
async function runTestInActivatedPage(testName) {
  const testChannel = new BroadcastChannel(`test-channel-${testName}`);
  testChannel.postMessage("activate");
  testChannel.close();

  return waitChannelMessage(`test-channel-${testName}`);
}
