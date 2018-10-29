// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Prepares the photo editor.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function setupPhotoEditor(testVolumeName, volumeType) {
  // Lauch the gallery.
  var launchedPromise = launch(
      testVolumeName,
      volumeType,
      [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var appId = args.appId;

    // Show the slide image.
    var slideImagePromise = gallery.waitForSlideImage(
        appId,
        800,
        600,
        'My Desktop Background');

    // Lauch the photo editor.
    var photoEditorPromise = slideImagePromise.then(function() {
      return gallery.waitAndClickElement(appId, 'button.edit');
    });

    return photoEditorPromise.then(function() {
      return args;
    });
  });
}

/**
 * Tests to rotate an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function rotateImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return gallery.waitAndClickElement(
        appId, '.gallery:not([locked]) button.rotate_right').then(function() {
          return gallery.waitForSlideImage(
              appId,
              600,
              800,
              'My Desktop Background');
        }).
        then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.rotate_left');
        }).
        then(function() {
          return gallery.waitForSlideImage(
              appId,
              800,
              600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests to crop an image and undoes it.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function cropImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    return gallery.waitAndClickElement(appId,
                                       '.gallery:not([locked]) button.crop').
        then(function() {
          return Promise.all([
            gallery.waitForElement(appId, '.crop-overlay')
          ]);
        }).
        then(function() {
          return gallery.fakeKeyDown(
              appId, 'body', 'Enter', false, false, false);
        }).
        then(function(ret) {
          chrome.test.assertTrue(ret);
          return Promise.all([
            gallery.waitForElementLost(appId, '.crop-overlay')
          ]);
        }).
        then(function() {
          return gallery.waitForSlideImage(
              appId,
              534,
              400,
              'My Desktop Background');
        }).
        then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.undo');
        }).
        then(function() {
           return gallery.waitForSlideImage(
              appId,
              800,
              600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests to exposure an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function exposureImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;
    var url = args.urls[0];
    var buttonQuery = '.gallery:not([locked]) button.exposure';
    var origMetadata = null;

    // Click the exposure button.
    return gallery.waitAndClickElement(appId, buttonQuery).then(function() {
      // Wait until the edit controls appear.
      return Promise.all([
        gallery.waitForElement(appId, '.brightness > paper-slider'),
        gallery.waitForElement(appId, '.contrast > paper-slider'),
      ]);
    }).then(function() {
      return gallery.callRemoteTestUtil(
          'changeValue', appId, ['.brightness > paper-slider', 20]);
    }).then(function() {
      return gallery.callRemoteTestUtil(
          'changeValue', appId, ['.contrast > paper-slider', -20]);
    }).then(function() {
      return gallery.callRemoteTestUtil('getMetadata', null, [url]);
    }).then(function(metadata) {
      origMetadata = metadata;

      // Push the Enter key.
      return gallery.fakeKeyDown(appId, 'body', 'Enter', false, false, false);
    }).then(function() {
      // Wait until the image is updated.
      return repeatUntil(function() {
        return gallery.callRemoteTestUtil('getMetadata', null, [url])
        .then(function(metadata) {
          if (origMetadata.modificationTime != metadata.modificationTime) {
            return true;
          } else {
            return pending(
                '%s is not updated. ' +
                    'First last modified: %s, Second last modified: %s.',
                url,
                origMetadata.modificationTime,
                metadata.modificationTime);
          }
        });
      });
    });
  });
}

/**
 * Tests to resize an image and undoes it.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function resizeImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appId = args.appId;

    return gallery
        .waitAndClickElement(appId, '.gallery:not([locked]) button.resize')
        .then(function() {
          return Promise.all([
            gallery.waitForElement(appId, '.width > cr-input'),
            gallery.waitForElement(appId, '.height > cr-input'),
            gallery.waitForElement(appId, '.lockicon[locked]'),
          ]);
        })
        .then(function() {
          return gallery.callRemoteTestUtil(
              'changeValue', appId, ['.height > cr-input', 500]);
        })
        .then(function() {
          return gallery.fakeKeyDown(
              appId, 'body', 'Enter', false, false, false);
        })
        .then(function() {
          return gallery.waitForSlideImage(appId, 667, 500,
              'My Desktop Background');
        })
        .then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.undo');
        })
        .then(function() {
          return gallery.waitForSlideImage(appId, 800, 600,
              'My Desktop Background');
        })
        .then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.resize');
        })
        .then(function() {
          return Promise.all([
            gallery.waitForElement(appId, '.width > cr-input'),
            gallery.waitForElement(appId, '.height > cr-input'),
            gallery.waitForElement(appId, '.lockicon[locked]'),
          ]);
        })
        .then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) .lockicon[locked]');
        })
        .then(function() {
          return gallery.callRemoteTestUtil(
              'changeValue', appId, ['.width > cr-input', 500]);
        })
        .then(function() {
          return gallery.callRemoteTestUtil(
              'changeValue', appId, ['.height > cr-input', 300]);
        })
        .then(function() {
          return gallery.fakeKeyDown(
              appId, 'body', 'Enter', false, false, false);
        })
        .then(function() {
          return gallery.waitForSlideImage(appId, 500, 300,
              'My Desktop Background');
        })
        .then(function() {
          return gallery.waitAndClickElement(
              appId, '.gallery:not([locked]) button.undo');
        })
        .then(function() {
          return gallery.waitForSlideImage(appId, 800, 600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests whether overwrite original checkbox is enabled or disabled properly.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function enableDisableOverwriteOriginalCheckbox(testVolumeName, volumeType) {
  var appId;
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(result) {
    appId = result.appId;

    // Confirm overwrite original checkbox is enabled and checked.
    return gallery.waitForElement(appId,
        '.overwrite-original[checked]:not([disabled])');
  }).then(function() {
    // Uncheck overwrite original.
    return gallery.waitAndClickElement(appId, '.overwrite-original');
  }).then(function() {
    // Rotate image.
    return gallery.waitAndClickElement(appId, '.rotate_right');
  }).then(function() {
    // Confirm that edited image has been saved.
    return gallery.waitForAFile(volumeType,
        'My Desktop Background - Edited.png');
  }).then(function() {
    // Confirm overwrite original checkbox is disabled and not checked.
    return gallery.waitForElement(appId,
        '.overwrite-original[disabled]:not([checked])');
  }).then(function() {
    // Go back to the slide mode.
    return gallery.waitAndClickElement(appId, 'button.edit');
  }).then(function() {
    // Confirm current image is My Desktop Background - Edited.png.
    return gallery.waitForSlideImage(appId, 600, 800,
        'My Desktop Background - Edited');
  }).then(function() {
    // Move to My Desktop Background.png. Switching to other image is required
    // to end edit session of the edited image.
    return gallery.waitAndClickElement(appId, '.arrow.right');
  }).then(function() {
    // Confirm current image has changed to another image.
    return gallery.waitForSlideImage(appId, 800, 600, 'My Desktop Background');
  }).then(function() {
    // Back to the edited image.
    return gallery.waitAndClickElement(appId, '.arrow.left');
  }).then(function() {
    // Confirm current image is switched to My Desktop Background - Edited.png.
    return gallery.waitForSlideImage(appId, 600, 800,
        'My Desktop Background - Edited');
  }).then(function() {
    // Click edit button again.
    return gallery.waitAndClickElement(appId, 'button.edit');
  }).then(function() {
    // Confirm overwrite original checkbox is enabled and not checked.
    return gallery.waitForElement(appId,
        '.overwrite-original:not([checked]):not([disabled])');
  });
}

/**
 * The rotateImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.rotateImageOnDownloads = function() {
  return rotateImage('local', 'downloads');
};

/**
 * The rotateImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.rotateImageOnDrive = function() {
  return rotateImage('drive', 'drive');
};

/**
 * The cropImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.cropImageOnDownloads = function() {
  return cropImage('local', 'downloads');
};

/**
 * The cropImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.cropImageOnDrive = function() {
  return cropImage('drive', 'drive');
};

/**
 * The exposureImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.exposureImageOnDownloads = function() {
  return exposureImage('local', 'downloads');
};

/**
 * The exposureImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.exposureImageOnDrive = function() {
  return exposureImage('drive', 'drive');
};

/**
 * The resize test for Downloas.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.resizeImageOnDownloads = function() {
  return resizeImage('local', 'downloads');
};

/**
 * The resize test for Google Drive
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.resizeImageOnDrive = function() {
  return resizeImage('drive', 'drive');
};

/**
 * The enableDisableOverwriteOriginalCheckbox test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.enableDisableOverwriteOriginalCheckboxOnDownloads = function() {
  return enableDisableOverwriteOriginalCheckbox('local', 'downloads');
};

/**
 * The enableDisableOverwriteOriginalCheckbox test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.enableDisableOverwriteOriginalCheckboxOnDrive = function() {
  return enableDisableOverwriteOriginalCheckbox('drive', 'drive');
};
