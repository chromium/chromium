// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Additional test image entry.
 */
ENTRIES.image4 = new TestEntryInfo({
  type: EntryType.FILE,
  sourceFileName: 'image3.jpg',
  targetPath: 'image4.jpg',
  mimeType: 'image/jpeg',
  lastModifiedTime: 'Jan 18, 2038, 1:02 AM',
  nameText: 'image3.jpg',
  sizeText: '3 KB',
  typeText: 'JPEG image'
});

/**
 * Renames an image in thumbnail mode and confirms that thumbnail of renamed
 * image is successfully updated.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function renameImageInThumbnailMode(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.desktop, ENTRIES.image3], [ENTRIES.desktop]);
  var appId;
  return launchedPromise.then(function(result) {
    // Confirm initial state after the launch.
    appId = result.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
    // Goes to thumbnail mode.
    return gallery.waitAndClickElement(appId, 'button.mode');
  }).then(function() {
    return gallery.selectImageInThumbnailMode(appId, 'image3.jpg');
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return gallery.callRemoteTestUtil('changeName', appId, ['New Image Name']);
  }).then(function() {
    // Assert that rename had done successfully.
    return gallery.waitForAFile(volumeType, 'New Image Name.jpg');
  }).then(function() {
    return gallery.selectImageInThumbnailMode(
        appId, 'My Desktop Background.png');
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(result) {
    // Only My Desktop Background.png is selected.
    chrome.test.assertEq(1, result.length);

    chrome.test.assertEq('My Desktop Background.png',
        result[0].attributes['title']);
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li:not(.selected)']);
  }).then(function(result) {
    // Confirm that thumbnail of renamed image has updated.
    chrome.test.assertEq(1, result.length);
    chrome.test.assertEq('New Image Name.jpg',
        result[0].attributes['title']);
  });
}

/**
 * Delete all images in thumbnail mode and confirm that no-images error banner
 * is shown.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {string} operation How the test do delete operation.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function deleteAllImagesInThumbnailMode(testVolumeName, volumeType, operation) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.desktop, ENTRIES.image3]);
  var appId;
  return launchedPromise.then(function(result) {
    appId = result.appId;
    // Wait until current mode is set to thumbnail mode.
    return gallery.waitForElement(appId, '.gallery[mode="thumbnail"]');
  }).then(function() {
    switch (operation) {
      case 'mouse':
        // Click delete button.
        return gallery.waitAndClickElement(appId, 'button.delete');
        break;
      case 'enter-key':
        // Press enter key on delete button.
        return gallery.waitForElement(
            appId, 'button.delete').then(function() {
          return gallery.callRemoteTestUtil(
              'focus', appId, ['button.delete']);
        }).then(function() {
          return gallery.callRemoteTestUtil(
              'fakeKeyDown', appId,
              ['button.delete', 'Enter', false, false, false]);
        }).then(function() {
          // When user has pressed enter key on button, click event is
          // dispatched after keydown event.
          return gallery.callRemoteTestUtil(
              'fakeEvent', appId, ['button.delete', 'click']);
        });
        break;
      case 'delete-key':
        // Press delete key.
        return gallery.callRemoteTestUtil(
            'fakeKeyDown', appId, ['body', 'Delete', false, false, false]);
        break;
    }
  }).then(function(result) {
    chrome.test.assertTrue(!!result);
    // Wait and click delete button of confirmation dialog.
    return gallery.waitAndClickElement(appId, '.cr-dialog-ok');
  }).then(function(result) {
    chrome.test.assertTrue(!!result);
    // Wait until error banner is shown.
    return gallery.waitForElement(appId, '.gallery[error] .error-banner');
  }).then(function() {
    // Check: The edit name field should hide.
    return gallery.waitForElement(appId, '#rename-input[hidden]');
  });
}

/**
 * Clicks an empty space in thumbnail view and confirms that current selection
 * is unselected.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function emptySpaceClickUnselectsInThumbnailMode(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.desktop, ENTRIES.image3], [ENTRIES.desktop]);
  var appId;
  return launchedPromise.then(function(result) {
    // Confirm initial state after the launch.
    appId = result.appId;
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function(result) {
    // Switch to thumbnail mode.
    return gallery.waitAndClickElement(appId, 'button.mode');
  }).then(function(result) {
    // Confirm My Desktop Background.png is selected in thumbnail view.
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('My Desktop Background.png',
        results[0].attributes['title']);
    // Click empty space of thumbnail view.
    return gallery.waitAndClickElement(appId, '.thumbnail-view > ul');
  }).then(function(result) {
    // Confirm no image is selected.
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(0, results.length);
    // Confirm delete button is disabled.
    return gallery.waitForElement(appId, 'button.delete[disabled]');
  }).then(function(result) {
    // Confirm slideshow button is disabled.
    return gallery.waitForElement(appId, 'button.slideshow[disabled]');
  }).then(function() {
    // Check: The edit name field should hide.
    return gallery.waitForElement(appId, '#rename-input[hidden]');
  }).then(function() {
    // Switch back to slide mode by clicking mode button.
    return gallery.waitAndClickElement(appId, 'button.mode:not([disabled])');
  }).then(function(result) {
    // First image in the image set (image3) should be shown.
    return gallery.waitForSlideImage(appId, 640, 480, 'image3');
  });
}

/**
 * Selects multiple images with shift key.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function selectMultipleImagesWithShiftKey(testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.image3, ENTRIES.image4, ENTRIES.desktop], [ENTRIES.image3]);
  var appId;
  return launchedPromise.then(function(result) {
    // Confirm initial state after the launch.
    appId = result.appId;
    return gallery.waitForSlideImage(appId, 640, 480, 'image3');
  }).then(function() {
    // Swith to thumbnail mode.
    return gallery.waitAndClickElement(appId, 'button.mode');
  }).then(function() {
    // Confirm that image3 is selected first: [1] 2  3
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('image3.jpg',
        results[0].attributes['title']);

    // Press Right key with shift.
    return gallery.fakeKeyDown(
        appId, '.thumbnail-view', 'ArrowRight', false, true /* Shift */, false);
  }).then(function() {
    // Confirm 2 images are selected: [1][2] 3
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(2, results.length);
    chrome.test.assertEq('image3.jpg', results[0].attributes['title']);
    chrome.test.assertEq('image4.jpg', results[1].attributes['title']);

    // Press Right key with shift.
    return gallery.fakeKeyDown(
        appId, '.thumbnail-view', 'ArrowRight', false, true /* Shift */, false);
  }).then(function() {
    // Confirm 3 images are selected: [1][2][3]
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(3, results.length);
    chrome.test.assertEq('image3.jpg', results[0].attributes['title']);
    chrome.test.assertEq('image4.jpg', results[1].attributes['title']);
    chrome.test.assertEq('My Desktop Background.png',
        results[2].attributes['title']);

    // Press Left key with shift.
    return gallery.fakeKeyDown(
        appId, '.thumbnail-view', 'ArrowLeft', false, true /* Shift */, false);
  }).then(function() {
    // Confirm 2 images are selected: [1][2] 3
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(2, results.length);
    chrome.test.assertEq('image3.jpg', results[0].attributes['title']);
    chrome.test.assertEq('image4.jpg', results[1].attributes['title']);

    // Press Right key without shift.
    return gallery.fakeKeyDown(
        appId, '.thumbnail-view', 'ArrowRight', false, false, false);
  }).then(function() {
    // Confirm only the last image is selected: 1  2 [3]
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('My Desktop Background.png',
        results[0].attributes['title']);
  });
}

/**
 * Selects all images in thumbnail mode after deleted an image in slide mode.
 * @param {string} testVolumeName Test volume name.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
function selectAllImagesAfterImageDeletionOnDownloads(
    testVolumeName, volumeType) {
  var launchedPromise = launch(testVolumeName, volumeType,
      [ENTRIES.image3, ENTRIES.image4, ENTRIES.desktop], [ENTRIES.image3]);
  var appId;
  return launchedPromise.then(function(result) {
    appId = result.appId;
    // Confirm initial state after launch.
    return gallery.waitForSlideImage(appId, 640, 480, 'image3');
  }).then(function() {
    // Delete an image.
    return gallery.waitAndClickElement(appId, 'button.delete');
  }).then(function() {
    // Press OK button in confirmation dialog.
    return gallery.waitAndClickElement(appId, '.cr-dialog-ok');
  }).then(function() {
    // Confirm the state after the image is deleted.
    return gallery.waitForSlideImage(appId, 640, 480, 'image4');
  }).then(function() {
    // Press thumbnail mode button.
    return gallery.waitAndClickElement(appId, 'button.mode');
  }).then(function() {
    // Confirm mode has been changed to thumbnail mode.
    return gallery.waitForElement(appId, '.gallery[mode="thumbnail"]');
  }).then(function() {
    // Press Ctrl+A to select all images.
    return gallery.fakeKeyDown(appId, '.thumbnail-view',
        'a', true /* Ctrl*/, false /* Shift */, false /* Alt */);
  }).then(function() {
    // Confirm that 2 images are selected.
    return gallery.callRemoteTestUtil('queryAllElements', appId,
        ['.thumbnail-view > ul > li.selected']);
  }).then(function(results) {
    chrome.test.assertEq(2, results.length);
  });
}

/**
 * Rename test in thumbnail mode for Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageInThumbnailModeOnDownloads = function() {
  return renameImageInThumbnailMode('local', 'downloads');
};

/**
 * Rename test in thumbnail mode for Drive.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.renameImageInThumbnailModeOnDrive = function() {
  return renameImageInThumbnailMode('drive', 'drive');
};

/**
 * Delete all images test in thumbnail mode for Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeOnDownloads = function() {
  return deleteAllImagesInThumbnailMode('local', 'downloads', 'mouse');
};

/**
 * Delete all images test in thumbnail mode for Drive.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeOnDrive = function() {
  return deleteAllImagesInThumbnailMode('drive', 'drive', 'mouse');
};

/**
 * Delete all images test in thumbnail mode by pressing Enter key on Delete
 * button.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeWithEnterKey = function() {
  return deleteAllImagesInThumbnailMode('local', 'downloads', 'enter-key');
};

/**
 * Delete all images test in thumbnail mode with Delete key.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.deleteAllImagesInThumbnailModeWithDeleteKey = function() {
  return deleteAllImagesInThumbnailMode('local', 'downloads', 'delete-key');
};

/**
 * Empty space click unselects current selection in thumbnail mode for
 * Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.emptySpaceClickUnselectsInThumbnailModeOnDownloads = function() {
  return emptySpaceClickUnselectsInThumbnailMode('local', 'downloads');
};

/**
 * Empty space click unselects current selection in thumbnail mode for Drive.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.emptySpaceClickUnselectsInThumbnailModeOnDrive = function() {
  return emptySpaceClickUnselectsInThumbnailMode('drive', 'drive');
};

/**
 * Selects multiple images with shift key in Downloads.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.selectMultipleImagesWithShiftKeyOnDownloads = function() {
  return selectMultipleImagesWithShiftKey('local', 'downloads');
};

/**
 * Selects all images in thumbnail mode after deleted an image in slide mode.
 * @return {!Promise} Promise to be fulfilled with on success.
 */
testcase.selectAllImagesAfterImageDeletionOnDownloads = function() {
  return selectAllImagesAfterImageDeletionOnDownloads('local', 'downloads');
};
