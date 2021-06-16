// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './check_elements.js';
import './click_control_buttons.js';
import './open_video_files.js';

import {RemoteCall} from '../remote_call.js';
import {addEntries, RootPath, sendBrowserTestCommand, testPromiseAndApps} from '../test_util.js';
import {testcase} from '../testcase.js';

/* eslint-disable no-var */

/**
 * Extension ID of the Files app.
 * @type {string}
 * @const
 */
export const VIDEO_PLAYER_APP_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

export const remoteCallVideoPlayer = new RemoteCall(VIDEO_PLAYER_APP_ID);

/**
 * Launches the video player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
export function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames = selectedEntries.map(function(entry) {
      return entry.nameText;
    });
    return remoteCallVideoPlayer.getFilesUnderVolume(
        volumeType, selectedEntryNames);
  });

  var appWindow = null;
  return entriesPromise
      .then(function(urls) {
        return remoteCallVideoPlayer.callRemoteTestUtil(
            'openVideoPlayer', null, [urls]);
      })
      .then(function(windowId) {
        appWindow = windowId;
        return remoteCallVideoPlayer.waitForElement(appWindow, 'body');
      })
      .then(function() {
        return Promise.all([
          remoteCallVideoPlayer.waitForElement(
              appWindow, '#video-player[first-video]'),
        ]);
      })
      .then(function(args) {
        return [appWindow, args[0]];
      });
}

/**
 * Open video player with multiple videos and subtitles.
 * @param {string} volumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Files to be opened.
 * @param {Array<TestEntryInfo>=} subtitles Subtitle files to be added to
 *     volume.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
export function openVideos(volumeName, volumeType, entries, subtitles = []) {
  const allEntries = entries.concat(subtitles);
  return launch(volumeName, volumeType, allEntries, entries).then((args) => {
    const videoPlayer = args[1];

    chrome.test.assertFalse('disabled' in videoPlayer.attributes);
    return args;
  });
}

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
      if (JSON.parse(mode) != chrome.extension.inIncognitoContext) {
        return;
      }
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
          return testPromiseAndApps(test(), [remoteCallVideoPlayer]);
        },
      };
      // Run the test.
      chrome.test.runTests([testCase[testCaseSymbol]]);
    }
  ];
  steps.shift()();
});
