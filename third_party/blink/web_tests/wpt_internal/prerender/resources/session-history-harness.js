// We don't have the test harness in this context, so we roll our own
// which communicates with our initiator which is actually running the tests.

function assert(condition, message) {
  if (!condition) {
    throw new Error("Assertion failed: " + message);
  }
}

if (document.prerendering) {
  window.onload = async () => {
    const urlParams = new URLSearchParams(window.location.search);
    const testName = urlParams.get("testName");
    const prerenderChannel = new BroadcastChannel(
      `prerender-channel-${testName}`,
    );

    // The document load event is not finished at this point, so navigations
    // would be done with replacement. This interferes with our tests. We wait
    // for the next task before navigating to avoid this.
    await new Promise((resolve) => {
      window.setTimeout(resolve);
    });

    try {
      let testFn = window[testName];
      if (!testFn) {
        prerenderChannel.postMessage("Missing test: " + testName);
        return;
      }
      testFn();
      prerenderChannel.postMessage("Passed");
    } catch (e) {
      prerenderChannel.postMessage(
        "Failed: " + e.name + ": " + e.message,
      );
    } finally {
      prerenderChannel.close();
    }
  };
}
