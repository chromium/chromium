// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Returns the title and artist text associated with the given audio track.
   *
   * @param {string} audioAppId The Audio Player window ID.
   * @param {query} track Query for the Audio Player track.
   * @return {Promise<Object>} Promise to be fulfilled with a track details
   *     object containing {title:string, artist:string}.
   */
  async function getTrackText(audioAppId, track) {
    await audioPlayerApp.waitForElement(audioAppId, trackListQuery(track));

    const title = await audioPlayerApp.callRemoteTestUtil(
        'deepQueryAllElements', audioAppId,
        [trackListQuery(track + ' > .data > .data-title')]);
    const artist = await audioPlayerApp.callRemoteTestUtil(
        'deepQueryAllElements', audioAppId,
        [trackListQuery(track + ' > .data > .data-artist')]);

    return {
      title: title[0] && title[0].text,
      artist: artist[0] && artist[0].text,
    };
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
  async function audioTimeLeapForward(audioAppId) {
    for (let i = 1; i <= 9; ++i) {
      await audioPlayerApp.fakeKeyDown(
          audioAppId, 'body', 'ArrowRight', false, false, false);
    }
  }

  /**
   * Tests opening then closing the Audio Player from Files app. Also tests that
   * an audio file from |path| opens and auto-plays.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioOpenClose(path) {
    const track = [ENTRIES.beautiful];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, track, track);

    // Open an audio file from |path|.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Close the Audio Player window.
    chrome.test.assertTrue(
        !!await audioPlayerApp.closeWindowAndWait(audioAppId),
        'Failed to close audio window');
  }

  /**
   * Tests an audio file opened from Downloads 1) auto-plays and 2) has the
   * correct track and artist details.
   */
  async function audioOpenTrackDownloads() {
    const track = [ENTRIES.beautiful];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on Downloads, add an audio file to Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, track, []);

    // Open an audio file from Downloads.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Check: track 0 should be active.
    let song = await getTrackText(audioAppId, '.track[index="0"][active]');

    // Check: track 0 should have the correct title and artist.
    chrome.test.assertEq('Beautiful Song', song.title);
    chrome.test.assertEq('Unknown Artist', song.artist);
  }

  /**
   * Tests an audio file opened from Drive 1) auto-plays and 2) has the correct
   * track and artist details. Tests the same when another Drive audio track is
   * opened from Files app.
   */
  async function audioOpenMultipleTracksDrive() {
    const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath, ENTRIES.newlyAdded.targetPath],
      openType: 'launch'
    });

    // Open Files.App on Drive, add the audio files to Drive.
    const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], tracks);

    // Open an audio file from Drive.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Check: track 0 should be active.
    let song = await getTrackText(audioAppId, '.track[index="0"][active]');

    // Check: track 0 should have the correct title and artist.
    chrome.test.assertEq('Beautiful Song', song.title);
    chrome.test.assertEq('Unknown Artist', song.artist);

    // Check: track 1 should be inactive.
    const inactive = trackListQuery('.track[index="1"]:not([active])');
    chrome.test.assertTrue(
        !!await audioPlayerApp.waitForElement(audioAppId, inactive));

    // Open another audio file from Drive.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['newly added file.ogg']));

    // Check: Audio Player should automatically play the file.
    const playFile2 = audioPlayingQuery('newly added file.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile2);

    // Check: track 1 should be active.
    song = await getTrackText(audioAppId, '.track[index="1"][active]');

    // Check: track 1 should have the correct title and artist.
    chrome.test.assertEq('newly added file', song.title);
    chrome.test.assertEq('Unknown Artist', song.artist);

    // Check: track 0 should be inactive.
    const inactive2 = trackListQuery('.track[index="0"]:not([active])');
    chrome.test.assertTrue(
        !!await audioPlayerApp.waitForElement(audioAppId, inactive2));
  }

  /**
   * Tests that the audio player auto-advances viz., auto-plays the next audio
   * track when the current track ends.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioAutoAdvance(path) {
    const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add audio files to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, tracks, tracks);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing.
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // When it ends, Audio Player should play the next file (advance).
    const playFile2 = audioPlayingQuery('newly added file.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile2);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioRepeatAllModeSingleFile(path) {
    const track = [ENTRIES.beautiful];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, track, track);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Click the repeat button for repeat-all.
    const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton]),
        'Failed to click the repeat button');

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should replay it (repeat-all).
    const repeats = playFile + '[playcount="1"]';
    await audioPlayerApp.waitForElement(audioAppId, repeats);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioNoRepeatModeSingleFile(path) {
    const track = [ENTRIES.beautiful];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, track, track);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should stop playing.
    const playStop = 'audio-player[playcount="1"]:not([playing])';
    await audioPlayerApp.waitForElement(audioAppId, playStop);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioRepeatOneModeSingleFile(path) {
    const track = [ENTRIES.beautiful];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.beautiful.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add an audio file to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, track, track);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['Beautiful Song.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Click the repeat button for repeat-all.
    const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton]),
        'Failed to click the repeat button');

    // Click the repeat button again for repeat-once.
    const repeatButton2 = controlPanelQuery(['repeat-button', '.repeat-all']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton2]),
        'Failed to click the repeat button');

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should replay it (repeat-once).
    const repeats = playFile + '[playcount="1"]';
    await audioPlayerApp.waitForElement(audioAppId, repeats);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioRepeatAllModeMultipleFile(path) {
    const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.newlyAdded.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add audio files to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, tracks, tracks);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['newly added file.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('newly added file.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Click the repeat button for repeat-all.
    const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton]),
        'Failed to click the repeat button');

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should play the next file.
    const playFile2 = audioPlayingQuery('Beautiful Song.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile2);

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // When it ends, Audio Player should replay the first file (repeat-all).
    await audioPlayerApp.waitForElement(audioAppId, playFile);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioNoRepeatModeMultipleFile(path) {
    const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.newlyAdded.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add audio files to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, tracks, tracks);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['newly added file.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('newly added file.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should stop playing.
    const playStop = 'audio-player[playcount="1"]:not([playing])';
    await audioPlayerApp.waitForElement(audioAppId, playStop);
  }

  /**
   * Tests if the audio player play the next file after the current file.
   *
   * @param {string} path Directory path to be tested.
   */
  async function audioRepeatOneModeMultipleFile(path) {
    const tracks = [ENTRIES.beautiful, ENTRIES.newlyAdded];

    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.newlyAdded.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add audio files to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, tracks, tracks);

    // Open an audio file.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, ['newly added file.ogg']));

    // Wait for the Audio Player window.
    const audioAppId = await audioPlayerApp.waitForWindow('audio_player.html');

    // Check: Audio Player should automatically play the file.
    const playFile = audioPlayingQuery('newly added file.ogg');
    await audioPlayerApp.waitForElement(audioAppId, playFile);

    // Click the repeat button for repeat-all.
    const repeatButton = controlPanelQuery(['repeat-button', '.no-repeat']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton]),
        'Failed to click the repeat button');

    // Click the repeat button again for repeat-once.
    const repeatButton2 = controlPanelQuery(['repeat-button', '.repeat-all']);
    chrome.test.assertTrue(
        await audioPlayerApp.callRemoteTestUtil(
            'fakeMouseClick', audioAppId, [repeatButton2]),
        'Failed to click the repeat button');

    // Leap forward in time.
    await audioTimeLeapForward(audioAppId);

    // Check: the same file should still be playing (non-repeated).
    const initial = playFile + '[playcount="0"]';
    await audioPlayerApp.waitForElement(audioAppId, initial);

    // When it ends, Audio Player should replay it (repeat-once).
    const repeats = playFile + '[playcount="1"]';
    await audioPlayerApp.waitForElement(audioAppId, repeats);
  }

  testcase.audioOpenCloseDownloads = () => {
    return audioOpenClose(RootPath.DOWNLOADS);
  };

  testcase.audioOpenCloseDrive = () => {
    return audioOpenClose(RootPath.DRIVE);
  };

  testcase.audioOpenDownloads = () => {
    return audioOpenTrackDownloads();
  };

  testcase.audioOpenDrive = () => {
    return audioOpenMultipleTracksDrive();
  };

  testcase.audioAutoAdvanceDrive = () => {
    return audioAutoAdvance(RootPath.DRIVE);
  };

  testcase.audioRepeatAllModeSingleFileDrive = () => {
    return audioRepeatAllModeSingleFile(RootPath.DRIVE);
  };

  testcase.audioNoRepeatModeSingleFileDrive = () => {
    return audioNoRepeatModeSingleFile(RootPath.DRIVE);
  };

  testcase.audioRepeatOneModeSingleFileDrive = () => {
    return audioRepeatOneModeSingleFile(RootPath.DRIVE);
  };

  testcase.audioRepeatAllModeMultipleFileDrive = () => {
    return audioRepeatAllModeMultipleFile(RootPath.DRIVE);
  };

  testcase.audioNoRepeatModeMultipleFileDrive = () => {
    return audioNoRepeatModeMultipleFile(RootPath.DRIVE);
  };

  testcase.audioRepeatOneModeMultipleFileDrive = () => {
    return audioRepeatOneModeMultipleFile(RootPath.DRIVE);
  };
})();
