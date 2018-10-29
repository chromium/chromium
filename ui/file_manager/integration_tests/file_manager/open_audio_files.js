// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Returns the title and artist text associated with the given audio track.
 *
 * @param {string} audioAppId The Audio Player window ID.
 * @param {query} track Query for the Audio Player track.
 * @return {Promise<Object>} Promise to be fulfilled with a track details
 *     object containing {title:string, artist:string}.
 */
function getTrackText(audioAppId, track) {
  const titleElement = audioPlayerApp.callRemoteTestUtil(
      'deepQueryAllElements', audioAppId,
      [trackListQuery(track + ' > .data > .data-title')]);
  const artistElement = audioPlayerApp.callRemoteTestUtil(
      'deepQueryAllElements', audioAppId,
      [trackListQuery(track + ' > .data > .data-artist')]);
  return Promise.all([titleElement, artistElement]).then((data) => {
    return {
      title: data[0][0] && data[0][0].text,
      artist: data[1][0] && data[1][0].text
    };
  });
}

/**
 * @param {string} query Query for an element inside <track-list> element.
 * @return {!Array<string>} deep query selector for an element inside
 *   <track-list> polymer element.
 */
function trackListQuery(query) {
  return ['audio-player', 'track-list', query];
}

/**
 * @param {!Array<string>|string} query Query for an element inside
 *     <control-panel> element.
 * @return {!Array<string>} Deep query selector for an element inside
 *   <control-panel> polymer element.
 */
function controlPanelQuery(query) {
  return ['audio-player', 'control-panel'].concat(query);
}

/*
 * Returns an Audio Player current track URL query for the given file name.
 *
 * @return {string} Track query for file name.
 */
function audioTrackQuery(fileName) {
  return '[currenttrackurl$="' + self.encodeURIComponent(fileName) + '"]';
}

/**
 * Returns a query for when the Audio Player is playing the given file name.
 *
 * @param {string} fileName The file name.
 * @return {string} Query for file name being played.
 */
function audioPlayingQuery(fileName) {
  return 'audio-player[playing]' + audioTrackQuery(fileName);
}

/**
 * Makes the current Audio Player track leap forward in time in 10% increments
 * to 90% of the track duration. This "leap-forward-in-time" effect works best
 * if called real-soon™ after the track starts playing.
 *
 * @param {string} audioAppId The Audio Player window ID.
 */
function audioTimeLeapForward(audioAppId) {
  for (let i = 1; i <= 9; ++i) {
    audioPlayerApp.fakeKeyDown(
        audioAppId, 'body', 'ArrowRight', false, false, false);
  }
}

/**
 * Tests opening then closing the Audio Player from Files app. Also tests that
 * an audio file from |path| opens and auto-plays.
 *
 * @param {string} path Directory path to be tested.
 */
function audioOpenClose(path) {
  let appId;
  let audioAppId;

  const track = [ENTRIES.beautiful];

  StepsRunner.run([
    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, track, track);
    },
    // Open an audio file from |path|.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Close the Audio Player window.
    function() {
      audioPlayerApp.closeWindowAndWait(audioAppId).then(this.next);
    },
    // Check: Audio Player window should close.
    function(result) {
      chrome.test.assertTrue(!!result, 'Failed to close audio window');
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests an audio file opened from Downloads 1) auto-plays and 2) has the
 * correct track and artist details.
 */
function audioOpenTrackDownloads() {
  let appId;
  let audioAppId;

  const track = [ENTRIES.beautiful];

  StepsRunner.run([
    // Open Files.App on Downloads, add an audio file to Downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next, track, []);
    },
    // Open an audio file from Downloads.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Check: track 0 should be active.
    function() {
      getTrackText(audioAppId, '.track[index="0"][active]').then(this.next);
    },
    // Check: track 0 should have the correct title and artist.
    function(song) {
      chrome.test.assertEq('Beautiful Song', song.title);
      chrome.test.assertEq('Unknown Artist', song.artist);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests an audio file opened from Drive 1) auto-plays and 2) has the correct
 * track and artist details. Tests the same when another Drive audio track is
 * opened from Files app.
 */
function audioOpenMultipleTracksDrive() {
  let appId;
  let audioAppId;

  const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

  StepsRunner.run([
    // Open Files.App on Drive, add the audio files to Drive.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next, [], tracks);
    },
    // Open an audio file from Drive.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Check: track 0 should be active.
    function() {
      getTrackText(audioAppId, '.track[index="0"][active]').then(this.next);
    },
    // Check: track 0 should have the correct title and artist.
    function(song) {
      chrome.test.assertEq('Beautiful Song', song.title);
      chrome.test.assertEq('Unknown Artist', song.artist);
      this.next();
    },
    // Check: track 1 should be inactive.
    function() {
      const inactive = trackListQuery('.track[index="1"]:not([active])');
      audioPlayerApp.waitForElement(audioAppId, inactive).then(this.next);
    },
    // Open another audio file from Drive.
    function(element) {
      chrome.test.assertTrue(!!element);
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(result) {
      chrome.test.assertTrue(result);
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Check: track 1 should be active.
    function() {
      getTrackText(audioAppId, '.track[index="1"][active]').then(this.next);
    },
    // Check: track 1 should have the correct title and artist.
    function(song) {
      chrome.test.assertEq('newly added file', song.title);
      chrome.test.assertEq('Unknown Artist', song.artist);
      this.next();
    },
    // Check: track 0 should be inactive.
    function() {
      const inactive = trackListQuery('.track[index="0"]:not([active])');
      audioPlayerApp.waitForElement(audioAppId, inactive).then(this.next);
    },
    function(element) {
      chrome.test.assertTrue(!!element);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests that the audio player auto-advances viz., auto-plays the next audio
 * track when the current track ends.
 *
 * @param {string} path Directory path to be tested.
 */
function audioAutoAdvance(path) {
  let appId;
  let audioAppId;

  const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

  StepsRunner.run([
    // Open Files.App on |path|, add audio files to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, tracks, tracks);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Leap forward in time.
    function() {
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing.
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // When it ends, Audio Player should play the next file (advance).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatAllModeSingleFile(path) {
  let appId;
  let audioAppId;

  const track = [ENTRIES.beautiful];

  StepsRunner.run([
    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, track, track);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Leap forward in time.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should replay it (repeat-all).
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      const repeats = playFile + '[playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, repeats).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatModeSingleFile(path) {
  let appId;
  let audioAppId;

  const track = [ENTRIES.beautiful];

  StepsRunner.run([
    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, track, track);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Leap forward in time.
    function() {
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should stop playing.
    function() {
      const playStop = 'audio-player[playcount="1"]:not([playing])';
      audioPlayerApp.waitForElement(audioAppId, playStop).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatOneModeSingleFile(path) {
  let appId;
  let audioAppId;

  const track = [ENTRIES.beautiful];

  StepsRunner.run([
    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, track, track);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Click the repeat button again for repeat-once.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      const repeatButton = controlPanelQuery(['repeat-button', '.repeat-all']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Leap forward in time.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should replay it (repeat-once).
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      const repeats = playFile + '[playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, repeats).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatAllModeMultipleFile(path) {
  let appId;
  let audioAppId;

  const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

  StepsRunner.run([
    // Open Files.App on |path|, add audio files to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, tracks, tracks);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Leap forward in time.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should play the next file.
    function() {
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Leap forward in time.
    function() {
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // When it ends, Audio Player should replay the first file (repeat-all).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatModeMultipleFile(path) {
  let appId;
  let audioAppId;

  const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

  StepsRunner.run([
    // Open Files.App on |path|, add audio files to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, tracks, tracks);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Leap forward in time.
    function() {
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should stop playing.
    function() {
      const playStop = 'audio-player[playcount="1"]:not([playing])';
      audioPlayerApp.waitForElement(audioAppId, playStop).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatOneModeMultipleFile(path) {
  let appId;
  let audioAppId;

  const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

  StepsRunner.run([
    // Open Files.App on |path|, add audio files to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, tracks, tracks);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Click the repeat button again for repeat-once.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      const repeatButton = controlPanelQuery(['repeat-button', '.repeat-all']);
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick', audioAppId, [repeatButton], this.next);
    },
    // Leap forward in time.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      audioTimeLeapForward(audioAppId);
      this.next();
    },
    // Check: the same file should still be playing (non-repeated).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      const initial = playFile + '[playcount="0"]';
      audioPlayerApp.waitForElement(audioAppId, initial).then(this.next);
    },
    // When it ends, Audio Player should replay it (repeat-once).
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      const repeats = playFile + '[playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, repeats).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.audioOpenCloseDownloads = function() {
  audioOpenClose(RootPath.DOWNLOADS);
};

testcase.audioOpenCloseDrive = function() {
  audioOpenClose(RootPath.DRIVE);
};

testcase.audioOpenDownloads = function() {
  audioOpenTrackDownloads();
};

testcase.audioOpenDrive = function() {
  audioOpenMultipleTracksDrive();
};

testcase.audioAutoAdvanceDrive = function() {
  audioAutoAdvance(RootPath.DRIVE);
};

testcase.audioRepeatAllModeSingleFileDrive = function() {
  audioRepeatAllModeSingleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatModeSingleFileDrive = function() {
  audioNoRepeatModeSingleFile(RootPath.DRIVE);
};

testcase.audioRepeatOneModeSingleFileDrive = function() {
  audioRepeatOneModeSingleFile(RootPath.DRIVE);
};

testcase.audioRepeatAllModeMultipleFileDrive = function() {
  audioRepeatAllModeMultipleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatModeMultipleFileDrive = function() {
  audioNoRepeatModeMultipleFile(RootPath.DRIVE);
};

testcase.audioRepeatOneModeMultipleFileDrive = function() {
  audioRepeatOneModeMultipleFile(RootPath.DRIVE);
};

})();
