// Helper functions to verify that origin trial checks work as expected in a
// document loaded via window.open().

const MESSAGE_DONE = 'done';
const MESSAGE_RESULT = 'result';

window.isTheOpenerForOTTest = !window.opener;

// Listens for messages from the target (opened window), to convert test results
// into asserts, and complete the test.
receiveMessageFromTarget = (event) => {
  if (event.data === MESSAGE_DONE) {
    done();
    return;
  }

  if (MESSAGE_RESULT in event.data) {
    test(function() {
      assert_true(event.data.result, event.data.description);
    }, 'Trial is enabled in document after window.open');
  }
}

// Sends any message from the target to the opener.
sendToOpener = (message) => {
  window.opener.postMessage(message, '*');
}

// Sends a test result from the target to the opener, capturing if the test
// passed, and a description.
sendTestResult = (passed, description) => {
  sendToOpener({result: passed, description});
}

// Performs setup needed to allow a window to be opened, and collect test
// results from the target (opened window).
setupWindowOpenTest = () => {
  // Only the opener needs to do any test setup.
  if (!window.isTheOpenerForOTTest) {
    return;
  }

  // Test will signal done, once all results have been collected from the
  // target window.
  setup({explicit_done: true});

  // Allow windows to be opened.
  if (window.testRunner) {
    testRunner.setPopupBlockingEnabled(false);
  }

  // Listener to collect results from the target window.
  window.addEventListener('message', receiveMessageFromTarget, false);
}

// Runs the tests in the target (opened window).
runTestInTarget = (run_test_func, test_description) => {
  if (window.isTheOpenerForOTTest) {
    return;
  }

  const passed = run_test_func();
  sendTestResult(passed, test_description);

  sendToOpener(MESSAGE_DONE);
}

// Opens the current document in a new window.
openCurrentAsTarget = () => {
  if (window.isTheOpenerForOTTest) {
    const url = window.location.pathname;
    const filename = url.substring(url.lastIndexOf('/') + 1);
    window.open(filename);
  }
}
