// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test of Audio Palyer window and initial elements.
 *
 * @param {string} volumeName Test volume name passed to the addEntries
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openAudioPlayer(volumeName, volumeType) {
  var test = launch(volumeName, volumeType, [ENTRIES.newlyAdded]);
  return test.then(function(args) {
    var appWindow = args[0];
    return Promise.all([
      remoteCallAudioPlayer.waitForElement(
          appWindow, [['audio-player', 'track-list']]),
      remoteCallAudioPlayer.waitForElement(
          appWindow, [['audio-player', 'control-panel']]),
      remoteCallAudioPlayer.waitForElement(
          appWindow, [['audio-player', 'audio']]),
    ]);
  });
}

/**
 * The open audio player test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openAudioOnDownloads = function() {
  return openAudioPlayer('local', 'downloads');
};

/**
 * The open audio player test for Drive
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openAudioOnDrive = function() {
  return openAudioPlayer('drive', 'drive');
};
