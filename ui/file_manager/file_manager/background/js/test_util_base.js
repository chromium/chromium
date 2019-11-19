// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

/**
 * Loaded at runtime and invoked by the external message listener to handle
 * remote call requests.
 * @type{?function(*, function(*): void)}
 */
test.util.executeTestMessage = null;

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = () => {
  let responsesWaitingForLoad = [];

  // Return true for asynchronous functions, which keeps the connection to the
  // caller alive; Return false for synchronous functions.
  chrome.runtime.onMessageExternal.addListener(
      (request, sender, sendResponse) => {
        /**
         * List of extension ID of the testing extension.
         * @type {Array<string>}
         * @const
         */
        const kTestingExtensionIds = [
          'oobinhbdbiehknkpbpejbbpdbkdjmoco',  // File Manager test extension.
          'ejhcmmdhhpdhhgmifplfmjobgegbibkn',  // Gallery test extension.
          'ljoplibgfehghmibaoaepfagnmbbfiga',  // Video Player test extension.
          'ddabbgbggambiildohfagdkliahiecfl',  // Audio Player test extension.
        ];

        // Check the sender.
        if (!sender.id || kTestingExtensionIds.indexOf(sender.id) === -1) {
          // Silently return.  Don't return false; that short-circuits the
          // propagation of messages, and there are now other listeners that
          // want to handle external messages.
          return;
        }

        // Set the global IN_TEST flag, so other components are aware of it.
        window.IN_TEST = true;

        // If testing functions are loaded just run the requested function.
        if (test.util.executeTestMessage !== null) {
          return test.util.executeTestMessage(request, sendResponse);
        }

        // Queue the request/response pair.
        const obj = {request, sendResponse};
        responsesWaitingForLoad.push(obj);

        // Only load the script with testing functions in the first request.
        if (responsesWaitingForLoad.length > 1) {
          return true;
        }

        // Asynchronously load the testing functions.
        let script = document.createElement('script');
        document.body.appendChild(script);

        script.onload = () => {
          // Run queued request/response pairs.
          responsesWaitingForLoad.forEach((queueObj) => {
            test.util.executeTestMessage(
                queueObj.request, queueObj.sendResponse);
          });
          responsesWaitingForLoad = [];
        };

        script.onerror = /** Event */ event => {
          console.error('Failed to load the run-time test script: ' + event);
          throw new Error('Failed to load the run-time test script: ' + event);
        };

        const kFileManagerExtension =
            'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';
        const kTestScriptUrl = kFileManagerExtension +
            '/background/js/runtime_loaded_test_util.js';
        console.log('Loading ' + kTestScriptUrl);
        script.src = kTestScriptUrl;
        return true;
      });
};
