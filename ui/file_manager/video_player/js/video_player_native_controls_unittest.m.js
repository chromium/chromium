// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://test/chai_assert.js';

import {MockChromeStorageAPI} from '../../base/js/mock_chrome.m.js';

import {NativeControlsVideoPlayer} from './video_player_native_controls.m.js';

/**
 * Helper function for creating an mock HTMLVideoElement.
 *
 * @param {string} src Source of the video element.
 * @param {number} duration Total duration of the video element.
 * @param {number} currentTime Current time of the video element.
 * @return {HTMLVideoElement} Mock HTMLVideoElement.
 */
function mockVideoElement(src, duration, currentTime) {
  return /** @type {!HTMLVideoElement} */ (
      {src: src, duration: duration, currentTime: currentTime, seekable: true});
}

/**
 * Test case for save and resume playback position when close and reopen video
 * player app.
 */
export function testSaveResumePlayback() {
  new MockChromeStorageAPI();

  const player = new NativeControlsVideoPlayer();

  const testCases = [
    mockVideoElement('test_1', 305, 150), mockVideoElement('test_2', 305, 14),
    mockVideoElement('test_3', 305, 300), mockVideoElement('test_4', 295, 150)
  ];

  /** @suppress {accessControls} */
  function setPlayerVideoElementForTest(videoElement) {
    player.videoElement_ = videoElement;
  }

  /** @suppress {accessControls} */
  function callRestorePlayStateForTest() {
    player.restorePlayState_();
  }

  testCases.forEach((videoElement) => {
    setPlayerVideoElementForTest(videoElement);

    // Simulate save position.
    const timeWhenSaved = videoElement.currentTime;
    player.savePosition(false);

    // Simulate reopen video file.
    videoElement.currentTime = 0;
    callRestorePlayStateForTest();

    const resumeTime =
        timeWhenSaved - NativeControlsVideoPlayer.RESUME_REWIND_SECONDS;
    const ratio = resumeTime / videoElement.duration;

    const expectedCurrentTime =
        (videoElement.duration <
             NativeControlsVideoPlayer.RESUME_THRESHOLD_SECONDS ||
         ratio < NativeControlsVideoPlayer.RESUME_MARGIN ||
         ratio > (1 - NativeControlsVideoPlayer.RESUME_MARGIN)) ?
        0 :
        resumeTime;

    assertEquals(videoElement.currentTime, expectedCurrentTime);
  });
}
