// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openVideos, remoteCallVideoPlayer} from './background.js';
import {waitForFunctionResult} from './click_control_buttons.js';

/* eslint-disable no-var */

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDownloads = function() {
  var test = openVideos('local', 'downloads', [ENTRIES.world]);
  return test
      .then(function() {
        // Video player starts playing given file automatically.
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Play will finish in 2 seconds (world.ogv is 2-second short movie.)
        return waitForFunctionResult('isPlaying', 'world.ogv', false);
      });
};

/**
 * The openSingleImage test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDrive = function() {
  var test = openVideos('drive', 'drive', [ENTRIES.world]);
  return test
      .then(function() {
        // Video player starts playing given file automatically.
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Play will finish in 2 seconds (world.ogv is 2-second short movie.)
        return waitForFunctionResult('isPlaying', 'world.ogv', false);
      });
};

/**
 * Test that if player can sccussfully search subtitle with same url.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openVideoWithSubtitle = async function() {
  await openVideos('local', 'downloads', [ENTRIES.world], [ENTRIES.subtitle]);
  await waitForFunctionResult('isPlaying', 'world.ogv', true);
  await waitForFunctionResult('hasSubtitle', 'world.ogv', true);
};

/**
 * Test that if player will ignore unrelated subtitle.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openVideoWithoutSubtitle = async function() {
  await openVideos('local', 'downloads', [ENTRIES.video], [ENTRIES.subtitle]);
  await waitForFunctionResult('isPlaying', 'video_long.ogv', true);
  await waitForFunctionResult('hasSubtitle', 'video_long.ogv', false);
};

/**
 * Test that if player will auto play next video and handle subtitles correctly.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleVideosOnDownloads = async function() {
  const args = await openVideos(
      'local', 'downloads', [ENTRIES.world, ENTRIES.video], [ENTRIES.subtitle]);
  const appId = args[0];

  // Video player should auto play first video.
  await waitForFunctionResult('isPlaying', 'world.ogv', true);
  await remoteCallVideoPlayer.waitForElement(
      appId, '#video-player[first-video]');
  await remoteCallVideoPlayer.waitForElement(
      appId, '#video-player:not([last-video])');
  await waitForFunctionResult('hasSubtitle', 'world.ogv', true);

  // Auto play next video when previous video ends.
  await waitForFunctionResult('isPlaying', 'video_long.ogv', true);
  await remoteCallVideoPlayer.waitForElement(
      appId, '#video-player:not([first-video])');
  await remoteCallVideoPlayer.waitForElement(
      appId, '#video-player[last-video]');

  // Subtitle should be cleared
  await waitForFunctionResult('hasSubtitle', 'video_long.ogv', false);
};
