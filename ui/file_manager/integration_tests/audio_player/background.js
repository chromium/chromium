// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of audio player.
 * @type {string}
 * @const
 */
var AUDIO_PLAYER_APP_ID = 'cjbfomnbifhcdnihkgipgfcihmgjfhbf';

var remoteCallAudioPlayer = new RemoteCall(AUDIO_PLAYER_APP_ID);

/**
 * Launches the audio player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the audio player element.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames =
        selectedEntries.map(function(entry) { return entry.nameText; });
    return remoteCallAudioPlayer.getFilesUnderVolume(
        volumeType, selectedEntryNames);
  });

  var appWindow = null;
  return entriesPromise.then(function(urls) {
    return remoteCallAudioPlayer.callRemoteTestUtil(
        'openAudioPlayer', null, [urls]);
  }).then(function(windowId) {
    appWindow = windowId;
    return remoteCallAudioPlayer.waitForElement(appWindow, 'body');
  }).then(function() {
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appWindow, 'audio-player[playing]'),
    ]);
  }).then(function(args) {
    return [appWindow, args[0]];
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
          return testPromiseAndApps(test(), [remoteCallAudioPlayer]);
        },
      };
      // Run the test.
      chrome.test.runTests([testCase[testCaseSymbol]]);
    }
  ];
  steps.shift()();
});
