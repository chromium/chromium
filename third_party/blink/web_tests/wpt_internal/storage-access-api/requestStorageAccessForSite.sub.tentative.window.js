// META: script=/storage-access-api/helpers.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
'use strict';

// Note that this file follows the pattern in:
// third_party/blink/web_tests/external/wpt/storage-access-api/requestStorageAccess.sub.window.js
//
// Some tests are run at the top-level, and an iframe is added to validate API
// behavior in that context.

// Prefix each test case with an indicator so we know what context they are run
// in if they are used in multiple iframes.
let testPrefix = 'top-level-context';

// Keep track of if we run these tests in a nested context, we don't want to
// recurse forever.
let topLevelDocument = true;

// The query string allows derivation of test conditions, like whether the tests
// are running in a top-level context.
let queryParams = window.location.search.substring(1).split('&');
queryParams.forEach(function(param, index) {
  if (param.toLowerCase() == 'rootdocument=false') {
    topLevelDocument = false;
  } else if (param.split('=')[0].toLowerCase() == 'testcase') {
    testPrefix = param.split('=')[1];
  }
});

// Common tests to run in all frames.
test(
    () => {
      assert_not_equals(document.requestStorageAccessForSite, undefined);
    },
    '[' + testPrefix +
        '] document.requestStorageAccessForSite() should be supported on the document interface');

if (topLevelDocument) {
  promise_test(
      t => {
        let promise = document.requestStorageAccessForSite('https://test.com');
        let description =
            'document.requestStorageAccessForSite() call without user gesture';
        return promise
            .then(t.unreached_func('Should have rejected: ' + description))
            .catch(function(e) {
              assert_equals(undefined, e, description);
            });
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForSite() should be rejected by default with no user gesture');

  // Create a test with a single-child same-origin iframe.
  // This will validate that calls to requestStorageAccessForSite are rejected
  // in non-top-level contexts.
  RunTestsInIFrame(
      './resources/requestStorageAccessForSite-iframe.html?testCase=same-origin-frame&rootdocument=false');

  promise_test(
      async t => {
        let access_promise = null;
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForSite(document.location.origin);
        };
        await ClickButtonWithGesture(testMethod);

        return access_promise;
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForSite() should be resolved when called properly with a user gesture and the same site');

  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForSite() call with bogus URL';
        let testMethod = function() {
          access_promise = document.requestStorageAccessForSite('bogus-url')
                               .then(t.unreached_func(
                                   'Should have rejected: ' + description))
                               .catch(function(e) {
                                 assert_equals(undefined, e, description);
                               });
          ;
        };
        await ClickButtonWithGesture(testMethod);

        return access_promise;
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForSite() should be rejected when called with an invalid site');

  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForSite() call with data URL';
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForSite('data:,Hello%2C%20World%21')
                  .then(
                      t.unreached_func('Should have rejected: ' + description))
                  .catch(function(e) {
                    assert_equals(undefined, e, description);
                  });
          ;
        };
        await ClickButtonWithGesture(testMethod);

        return access_promise;
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForSite() should be rejected when called with an opaque origin');

} else {
  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForSite() call in a non-top-level context';
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForSite(document.location.origin)
                  .then(
                      t.unreached_func('Should have rejected: ' + description))
                  .catch(function(e) {
                    assert_equals(undefined, e, description);
                  });
          ;
        };
        await ClickButtonWithGesture(testMethod);

        return access_promise;
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForSite() should be rejected when called in an iframe');
}
