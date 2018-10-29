// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of gallery app.
 * @type {string}
 * @const
 */
var GALLERY_APP_ID = 'nlkncpkkdoccmpiclbokaimcnedabhhm';

var gallery = new RemoteCallGallery(GALLERY_APP_ID);

/**
 * Launches the gallery with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the data of the main element
 *     in the allery.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames =
        selectedEntries.map(function(entry) { return entry.nameText; });
    return gallery.callRemoteTestUtil(
        'getFilesUnderVolume', null, [volumeType, selectedEntryNames]);
  });

  var appId = null;
  var urls = [];
  return entriesPromise.then(function(result) {
    urls = result;
    return gallery.callRemoteTestUtil('openGallery', null, [urls]);
  }).then(function(windowId) {
    chrome.test.assertTrue(!!windowId);
    appId = windowId;
    return gallery.waitForElement(appId, 'div.gallery');
  }).then(function(args) {
    return {
      appId: appId,
      mailElement: args[0],
      urls: urls,
    };
  });
}

/**
 * Namespace for test cases.
 */
var testcase = {};

/**
 * When the FileManagerBrowserTest harness loads this test extension, request
 * configuration and other details from that harness, including the test case
 * name to run. Use the configuration/details to setup the test ennvironment,
 * then run the test case using chrome.test.RunTests.
 */
window.addEventListener('load', function() {
  var steps = [
    // Request the guest mode state.
    function() {
      sendBrowserTestCommand({name: 'isInGuestMode'}, steps.shift());
    },
    // Request the root entry paths.
    function(mode) {
      if (JSON.parse(mode) != chrome.extension.inIncognitoContext)
        return;
      sendBrowserTestCommand({name: 'getRootPaths'}, steps.shift());
    },
    // Request the test case name.
    function(paths) {
      var roots = JSON.parse(paths);
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.DRIVE = roots.drive;
      sendBrowserTestCommand({name: 'getTestName'}, steps.shift());
    },
    // Run the test case.
    function(testCaseName) {
      // Get the test function from testcase namespace testCaseName.
      var test = testcase[testCaseName];
      // Verify test is an unnamed (aka 'anonymous') Function.
      if (!(test instanceof Function) || test.name) {
        chrome.test.fail('[' + testCaseName + '] not found.');
        return;
      }
      // Define the test case and its name for chrome.test logging.
      test.generatedName = testCaseName;
      var testCaseSymbol = Symbol(testCaseName);
      var testCase = {
        [testCaseSymbol] :() => {
          return testPromiseAndApps(test(), [gallery]);
        },
      };
      // Run the test.
      chrome.test.runTests([testCase[testCaseSymbol]]);
    }
  ];
  steps.shift()();
});
