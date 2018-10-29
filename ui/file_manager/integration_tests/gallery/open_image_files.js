// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs a test to open a single image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleImage(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType, [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var WIDTH = 880;
    var HEIGHT = 602; /* Inner height 570px + native header 32px. */
    var appId = args.appId;
    var resizedWindowPromise = gallery.callRemoteTestUtil(
        'resizeWindow', appId, [WIDTH, HEIGHT]
    ).then(function() {
      return repeatUntil(function() {
        return gallery.callRemoteTestUtil('getWindows', null, []
        ).then(function(windows) {
          var bounds = windows[appId];
          if (!bounds)
            return pending('Window is not ready yet.');

          if (bounds.outerWidth !== WIDTH || bounds.outerHeight !== HEIGHT) {
            return pending(
                'Window bounds is expected %d x %d, but is %d x %d',
                WIDTH, HEIGHT,
                bounds.outerWidth,
                bounds.outerHeight);
          }
          return true;
        });
      });
    });

    return resizedWindowPromise.then(function() {
      var rootElementPromise =
          gallery.waitForElement(appId, '.gallery[mode="slide"]');
      var fullImagePromsie = gallery.waitForElementStyles(
          appId, '.gallery .image-container > .image', ['any']);
      return Promise.all([rootElementPromise, fullImagePromsie]).
          then(function(args) {
            chrome.test.assertEq(760, args[1].renderedWidth);
            chrome.test.assertEq(570, args[1].renderedHeight);
            chrome.test.assertEq(800, args[1].imageWidth);
            chrome.test.assertEq(600, args[1].imageHeight);
          });
    });
  });
}

/**
 * Confirms that two images are loaded in thumbnail mode. This method doesn't
 * care whether two images are loaded with error or not.
 *
 * @param {string} appId
 * @return {Promise} Promise to be fulfilled with on success.
 */
function confirmTwoImagesAreLoadedInThumbnailMode(appId) {
  // Wait until Gallery changes to thumbnail mode.
  return gallery.waitForElement(
      appId, '.gallery[mode="thumbnail"]').then(function() {
    // Confirm that two tiles are shown.
    return repeatUntil(function() {
      return gallery.callRemoteTestUtil('queryAllElements', appId,
          ['.thumbnail-view .thumbnail']).then(function(tiles) {
        if (tiles.length !== 2)
          return pending('The number of tiles is expected 2, but is %d',
              tiles.length);
        return tiles;
      });
    });
  });
}

/**
 * Runs a test to open multiple images.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openMultipleImages(testVolumeName, volumeType) {
  var testEntries = [ENTRIES.desktop, ENTRIES.image3];
  var launchedPromise = launch(testVolumeName, volumeType, testEntries);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return confirmTwoImagesAreLoadedInThumbnailMode(appId);
  });
}

/**
 * Runs a test to open multiple images and change to slide mode with keyboard.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openMultipleImagesAndChangeToSlideMode(testVolumeName, volumeType) {
  var testEntries = [ENTRIES.desktop, ENTRIES.image3];
  var launchedPromise = launch(testVolumeName, volumeType, testEntries);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return confirmTwoImagesAreLoadedInThumbnailMode(appId).then(function() {
      // Press Enter key and mode should be changed to slide mode.
      return gallery.callRemoteTestUtil(
          'fakeKeyDown', appId,
          [null /* active element */, 'Enter', false, false, false]);
    }).then(function() {
      // Wait until it changes to slide mode.
      return gallery.waitForElement(appId, '.gallery[mode="slide"]');
    });
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleImageOnDownloads = function() {
  return openSingleImage('local', 'downloads');
};

/**
 * The openSingleImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleImageOnDrive = function() {
  return openSingleImage('drive', 'drive');
};

/**
 * The openMultiImages test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleImagesOnDownloads = function() {
  return openMultipleImages('local', 'downloads');
};

/**
 * The openMultiImages test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleImagesOnDrive = function() {
  return openMultipleImages('drive', 'drive');
};

/**
 * The openMultipleImagesAndChangeToSlideMode test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openMultipleImagesAndChangeToSlideModeOnDownloads = function() {
  return openMultipleImagesAndChangeToSlideMode('local', 'downloads');
};

/**
 * Runs a test to check whether the rename-input field is hidden after
 * deleting the only selected image in the gallery.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.deleteSingleOpenPhotoOnDownloads = () => {
  const launchedPromise = launch('local', 'downloads', [ENTRIES.desktop]);
  let appId;
  return launchedPromise.then(args => {
    appId = args.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(() => {
    // Click the delete button.
    return gallery.waitAndClickElement(appId, 'button.delete');
  }).then(result => {
      chrome.test.assertTrue(!!result);
    // Wait and click delete button of confirmation dialog.
    return gallery.waitAndClickElement(appId, '.cr-dialog-ok');
  }).then(() => {
    // Check: The edit name field should hide.
    return gallery.waitForElement(appId, '#rename-input[hidden]');
  });
};