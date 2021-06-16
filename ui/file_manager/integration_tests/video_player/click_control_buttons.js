// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openVideos, remoteCallVideoPlayer} from './background.js';

/* eslint-disable no-var */

/**
 * Waits that calling callRemoteTestUtil for |funcName| function with |filename|
 * returns |expectedResult|.
 * @param {string} funcName Function name for callRemoteTestUtil.
 * @param {string} filename File name to pass to callRemoteTestUtil.
 * @param {*} expectedResult Expected result for the remote call.
 * @return {Promise} Promise which will be fullfiled when the expected result is
 *     given.
 */
export function waitForFunctionResult(funcName, filename, expectedResult) {
  var caller = getCaller();
  return repeatUntil(function() {
    return remoteCallVideoPlayer.callRemoteTestUtil(funcName, null, [filename])
        .then(function(result) {
          if (result === expectedResult) {
            return true;
          }
          return pending(
              caller, 'Waiting for %s return %s.', funcName, expectedResult);
        });
  });
}

/**
 * Confirms that native media keys are dispatched correctly.
 * @return {Promise} Promise to be fulfilled on success.
 */
testcase.mediaKeyNative = function() {
  const openVideo = openVideos('local', 'downloads', [ENTRIES.video]);
  let appId;
  function ensurePlaying() {
    return waitForFunctionResult('isPlaying', 'video_long.ogv', true);
  }
  function ensurePaused() {
    return waitForFunctionResult('isPlaying', 'video_long.ogv', false);
  }
  function sendMediaKey() {
    return sendTestMessage({name: 'dispatchNativeMediaKey'}).then((result) => {
      chrome.test.assertEq(
          result, 'mediaKeyDispatched', 'Key dispatch failure');
    });
  }
  function pauseAndUnpause() {
    // Video player should be playing when this is called,
    return Promise.resolve()
        .then(ensurePlaying)
        .then(sendMediaKey)
        .then(ensurePaused)
        .then(sendMediaKey)
        .then(ensurePlaying);
  }
  function enableTabletMode() {
    return sendTestMessage({name: 'enableTabletMode'}).then((result) => {
      chrome.test.assertEq(result, 'tabletModeEnabled');
    });
  }
  return openVideo
      .then((args) => {
        appId = args[0];
      })
      .then(pauseAndUnpause)
      .then(enableTabletMode)
      .then(pauseAndUnpause);
};
