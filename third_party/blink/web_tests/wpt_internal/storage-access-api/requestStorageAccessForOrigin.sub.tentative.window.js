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

// TODO(crbug.com/1351540): when/if requestStorageAccessForOrigin is standardized,
// upstream with the Storage Access API helpers file.
function RunRequestStorageAccessForOriginInDetachedFrame(site) {
  let nestedFrame = document.createElement('iframe');
  document.body.append(nestedFrame);
  const inner_doc = nestedFrame.contentDocument;
  nestedFrame.remove();
  return inner_doc.requestStorageAccessForOrigin(site);
}

function RunRequestStorageAccessForOriginViaDomParser(site) {
  let parser = new DOMParser();
  let doc = parser.parseFromString('<html></html>', 'text/html');
  return doc.requestStorageAccessForOrigin(site);
}

// Common tests to run in all frames.
test(
    () => {
      assert_not_equals(document.requestStorageAccessForOrigin, undefined);
    },
    '[' + testPrefix +
        '] document.requestStorageAccessForOrigin() should be supported on the document interface');

if (topLevelDocument) {
  promise_test(
      t => {
        let promise = document.requestStorageAccessForOrigin('https://test.com');
        let description =
            'document.requestStorageAccessForOrigin() call without user gesture';
        return promise
            .then(t.unreached_func('Should have rejected: ' + description))
            .catch(function(e) {
              assert_equals(undefined, e, description);
            });
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForOrigin() should be rejected by default with no user gesture');

  promise_test(async t => {
    let promise =
        RunRequestStorageAccessForOriginInDetachedFrame('https://foo.com');
    let description =
        'document.requestStorageAccessForOrigin() call in a detached frame';
    return promise
        .then(t.unreached_func('Should have rejected: ' + description))
        .catch(function(e) {
          assert_equals(e.name, 'InvalidStateError', description);
        });
  }, '[non-fully-active] document.requestStorageAccessForOrigin() should not resolve when run in a detached frame');

  promise_test(async t => {
    let promise = RunRequestStorageAccessForOriginViaDomParser('https://foo.com');
    let description =
        'document.requestStorageAccessForOrigin() in a detached DOMParser result';
    return promise
        .then(t.unreached_func('Should have rejected: ' + description))
        .catch(function(e) {
          assert_equals(e.name, 'InvalidStateError', description);
        });
  }, '[non-fully-active] document.requestStorageAccessForOrigin() should not resolve when run in a detached DOMParser document');

  // Create a test with a single-child same-origin iframe.
  // This will validate that calls to requestStorageAccessForOrigin are rejected
  // in non-top-level contexts.
  RunTestsInIFrame(
      './resources/requestStorageAccessForOrigin-iframe.html?testCase=same-origin-frame&rootdocument=false');

  promise_test(
      async t => {
        let access_promise = null;
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForOrigin(document.location.origin);
        };
        await ClickButtonWithGesture(testMethod);

        return access_promise;
      },
      '[' + testPrefix +
          '] document.requestStorageAccessForOrigin() should be resolved when called properly with a user gesture and the same site');

  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForOrigin() call with bogus URL';
        let testMethod = function() {
          access_promise = document.requestStorageAccessForOrigin('bogus-url')
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
          '] document.requestStorageAccessForOrigin() should be rejected when called with an invalid site');

  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForOrigin() call with data URL';
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForOrigin('data:,Hello%2C%20World%21')
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
          '] document.requestStorageAccessForOrigin() should be rejected when called with an opaque origin');

} else {
  promise_test(
      async t => {
        let access_promise = null;
        let description =
            'document.requestStorageAccessForOrigin() call in a non-top-level context';
        let testMethod = function() {
          access_promise =
              document.requestStorageAccessForOrigin(document.location.origin)
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
          '] document.requestStorageAccessForOrigin() should be rejected when called in an iframe');
}
