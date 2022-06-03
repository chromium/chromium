// Helpers to ease the transition of Geolocation API tests from the legacy
// js-test framework to testharness. This implements a subset of the js-test
// framework in terms of testharness primitives.

var testPassed;
var testFailed;
var finishJSTest;

// Actually defines a testharness test using the provided description.
function description(desc) {
  let passed = true;
  let failureReason;

  // NOTE: All geolocation API js-test steps are expected to PASS, so we can
  // essentially treat testPassed invocations as no-ops. A test only fails if
  // testFailed is ever invoked, and unlike the js-test framework we simply
  // grab the first failure and use that as the overall testharness test result.
  testPassed = () => {};
  testFailed = reason => {
    if (passed !== false) {
      // Only preserve the reason for the first failure.
      failureReason = reason;
    }
    passed = false;
  };

  const promise = new Promise((resolve, reject) => {
    finishJSTest = () => {
      if (passed) {
        resolve();
      } else {
        reject(failureReason);
      }
    };
  });

  promise_test(() => promise, desc);
}

function shouldBe(x, y) { assert_equals(eval(x), eval(y)); }
function shouldBeTrue(x) { assert_true(eval(x)); }
function shouldBeFalse(x) { assert_false(eval(x)); }
