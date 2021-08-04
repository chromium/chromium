// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals} from 'chrome://test/chai_assert.js';

import {MockChromeStorageAPI} from '../../common/js/mock_chrome.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {xfm} from '../../common/js/xfm.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {BannerController} from './banner_controller.js';
import {DirectoryModel} from './directory_model.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';

/** @type {!BannerController} */
let controller;

/** @type {!DirectoryModel} */
let directoryModel;

/** @type {!Element} */
let bannerContainer;

/**
 * @typedef {{
 *    setAllowedVolumeTypes: function(!Array<!Banner.AllowedVolumeType>),
 *    setShowLimit: function(number),
 *    reset: function(),
 *    tagName: string,
 * }}
 */
let TestBanner;

/**
 * Maintains the registered web components for testing the warning banners.
 * @type {!Array<!TestBanner>}
 */
const testWarningBanners = [];

/**
 * Maintains the registered web components for testing the educational banners.
 * @type {!Array<!TestBanner>}
 */
const testEducationalBanners = [];

/**
 * Number of banners to create on startup.
 * @type {number}
 */
const BANNERS_COUNT = 5;

/**
 * @type {!Banner.AllowedVolumeType}
 */
const downloadsAllowedVolumeType = {
  type: VolumeManagerCommon.VolumeType.DOWNLOADS,
  id: null,
};

/**
 * @type {!Banner.AllowedVolumeType}
 */
const driveAllowedVolumeType = {
  type: VolumeManagerCommon.VolumeType.DRIVE,
  id: null,
};

/**
 * @type {!Banner.AllowedVolumeType}
 */
const androidFilesAllowedVolumeType = {
  type: VolumeManagerCommon.VolumeType.ANDROID_FILES,
  id: null,
};

/**
 * Returns an object with helper methods to manipulate a test banner.
 * @return {!TestBanner}
 */
function createTestBanner(tagName) {
  /** @type {!Array<!Banner.AllowedVolumeType>} */
  let allowedVolumeTypes = [];

  /** @type {number|undefined} */
  let showLimit;

  class FakeBanner extends Banner {
    allowedVolumeTypes() {
      return allowedVolumeTypes;
    }

    showLimit() {
      return showLimit;
    }
  }

  customElements.define(tagName, FakeBanner);

  return {
    setAllowedVolumeTypes: (types) => {
      allowedVolumeTypes = types;
    },
    reset: () => {
      allowedVolumeTypes = [];
      showLimit = undefined;
    },
    setShowLimit: (limit) => {
      showLimit = limit;
    },
    // Element.tagName returns uppercase.
    tagName: tagName.toUpperCase(),
  };
}

/**
 * Asserts that a banner is attached and not hidden in the bannerContainer.
 * @param {!TestBanner} banner The banner to assert is visible.
 */
function assertBannerVisible(banner) {
  const visibleBanner = document.querySelector(banner.tagName);
  assertNotEquals(visibleBanner.getAttribute('hidden'), 'true');
  assertNotEquals(visibleBanner.getAttribute('aria-hidden'), 'true');

  // Verify if any other banners have been appended to the DOM, they are
  // hidden.
  for (const element of bannerContainer.children) {
    if (element !== visibleBanner) {
      assertEquals(element.getAttribute('hidden'), 'true');
      assertEquals(element.getAttribute('aria-hidden'), 'true');
    }
  }
}

/**
 * Asserts that all banners on the DOM are hidden.
 */
function assertNoVisibleBanners() {
  for (const element of bannerContainer.children) {
    assertEquals(element.getAttribute('hidden'), 'true');
    assertEquals(element.getAttribute('aria-hidden'), 'true');
  }
}

/**
 * Changes the global directory to |newVolume|.
 * @param {?VolumeManagerCommon.VolumeType} volumeType
 * @param {?string} volumeId
 */
function changeCurrentVolume(volumeType, volumeId) {
  directoryModel.getCurrentVolumeInfo = function() {
    // Certain directory roots return null (USB drive root).
    if (!volumeType) {
      return null;
    }
    return /** @type {!VolumeInfo} */ ({
      volumeType,
      volumeId,
    });
  };
}

export function setUpPage() {
  bannerContainer = document.createElement('div');
  bannerContainer.setAttribute('id', 'banners');
  document.body.appendChild(bannerContainer);

  for (let i = 0; i < BANNERS_COUNT; i++) {
    // Register and keep track of test warning banners.
    testWarningBanners.push(createTestBanner('warning-banner-' + i));

    // Register and keep track of test educational banners.
    testEducationalBanners.push(createTestBanner('educational-banner-' + i));
  }
}

export function setUp() {
  assertEquals(bannerContainer.childElementCount, 0);

  new MockChromeStorageAPI();
  xfm.storage.local.clear();
  directoryModel = createFakeDirectoryModel();
  controller = new BannerController(directoryModel);

  // Set the default navigated directory to DOWNLOADS.
  changeCurrentVolume(
      VolumeManagerCommon.VolumeType.DOWNLOADS, /* volumeId */ null);
}

export function tearDown() {
  bannerContainer.textContent = '';

  for (let i = 0; i < BANNERS_COUNT; i++) {
    testWarningBanners[i].reset();
    testEducationalBanners[i].reset();
  }
}

/**
 * Test that upon initialize (with no banners loaded) the container has none
 * appended.
 */
export async function testNoBanners() {
  await controller.initialize();
  assertEquals(bannerContainer.childElementCount, 0);
}

/**
 * Test that a warning banner takes priority over any educational banners.
 */
export async function testWarningBannerTopPriority() {
  // Add 1 warning and 1 educational banner to the controller.
  controller.setWarningBannersInOrder([testWarningBanners[0].tagName]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0].tagName]);

  // Set the allowed volume type to be the DOWNLOADS directory.
  testWarningBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);
  testEducationalBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  await controller.initialize();
  assertBannerVisible(testWarningBanners[0]);
}

/**
 * Test a banner with lower priority can be shown if the higher priority banner
 * does not match the current volume type.
 */
export async function testNextMatchingBannerShows() {
  // Add 1 warning and 1 educational banner to the controller.
  controller.setWarningBannersInOrder([testWarningBanners[0].tagName]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0].tagName]);

  // Only set the educational banner to have allowed type of DOWNLOADS.
  testEducationalBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  await controller.initialize();
  assertBannerVisible(testEducationalBanners[0]);
}

/**
 * Test banners of the same types are prioritised by the order they are set.
 */
export async function testBannersArePrioritisedByIndexOrder() {
  // Add 2 warning banners.
  controller.setWarningBannersInOrder([
    testWarningBanners[0].tagName,
    testWarningBanners[1].tagName,
  ]);

  // Set the banner at index 1 to be shown.
  testWarningBanners[1].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  await controller.initialize();
  assertBannerVisible(testWarningBanners[1]);
}

/**
 * Test banners of the same types are prioritised by the order they are set.
 */
export async function testBannersAreHiddenOnVolumeChange() {
  // Add 2 warning banners and 1 educational banner.
  controller.setWarningBannersInOrder([
    testWarningBanners[0].tagName,
    testWarningBanners[1].tagName,
  ]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0].tagName]);

  // Set the following banners to show:
  //  First Warning Banner shows on Downloads.
  //  Second Warning Banner shows on Drive.
  //  First Educational Banner shows on Android Files.
  testWarningBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);
  testWarningBanners[1].setAllowedVolumeTypes([driveAllowedVolumeType]);
  testEducationalBanners[0].setAllowedVolumeTypes(
      [androidFilesAllowedVolumeType]);

  // Verify for Downloads the first banner shows.
  await controller.initialize();
  assertBannerVisible(testWarningBanners[0]);

  // Change volume to Drives and verify the second warning banner is showing.
  changeCurrentVolume(
      VolumeManagerCommon.VolumeType.DRIVE, /* volumeId */ null);
  await controller.reconcile();
  assertBannerVisible(testWarningBanners[1]);

  // Change volume to Android files and verify the educational banner is shown.
  changeCurrentVolume(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, /* volumeId */ null);
  await controller.reconcile();
  assertBannerVisible(testEducationalBanners[0]);
}

/**
 * Test that a null VolumeInfo hides all the banners.
 */
export async function testNullVolumeInfoClearsBanners() {
  controller.setWarningBannersInOrder([testWarningBanners[0].tagName]);
  testWarningBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  // Verify for Downloads the warning banner is shown.
  await controller.initialize();
  assertBannerVisible(testWarningBanners[0]);

  // Change current volume to a null volume type and assert no banner is shown.
  changeCurrentVolume(/* volumeType */ null, /* volumeId */ null);
  await controller.reconcile();
  assertNoVisibleBanners();
}

/**
 * Test that banners that have hit their show limit are not shown again.
 */
export async function testBannersChangeAfterShowLimitReached() {
  // Add 2 educational banners.
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0].tagName,
    testEducationalBanners[1].tagName,
  ]);

  // Set the showLimit for the educational banners.
  testEducationalBanners[0].setShowLimit(1);
  testEducationalBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);
  testEducationalBanners[1].setShowLimit(3);
  testEducationalBanners[1].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  // The first reconciliation should increment the counter and append to DOM.
  await controller.initialize();
  assertBannerVisible(testEducationalBanners[0]);

  // Clearing the DOM should imitate a new Files app session.
  bannerContainer.textContent = '';
  await controller.reconcile();

  // After a new Files app session has started, the previous counter has
  // exceeded it's show limit, assert the next priority banner is shown.
  assertBannerVisible(testEducationalBanners[1]);
}

/**
 * Test that the show limit is increased only on a per app session.
 */
export async function testChangingVolumesDoesntIncreaseShowTimes() {
  // Add 1 educational banner.
  controller.setEducationalBannersInOrder([testEducationalBanners[0].tagName]);

  const bannerShowLimit = 3;
  testEducationalBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);
  testEducationalBanners[0].setShowLimit(bannerShowLimit);

  await controller.initialize();
  assertBannerVisible(testEducationalBanners[0]);

  // Change directory one more times than the show limit to verify the show
  // limit doesn't increase.
  for (let i = 0; i < bannerShowLimit + 1; i++) {
    // Change directory and verify no banner shown.
    changeCurrentVolume(VolumeManagerCommon.VolumeType.DRIVE, null);
    await controller.reconcile();
    assertNoVisibleBanners();

    // Change back to DOWNLOADS and verify banner has been shown.
    changeCurrentVolume(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
    await controller.reconcile();
    assertBannerVisible(testEducationalBanners[0]);
  }
}

/**
 * Test that multiple banners with different allowedVolumeTypes and show limits
 * are show at the right stages. This also asserts that banners that don't
 * implement showLimit still are shown as expected.
 */
export async function testMultipleBannersAllowedVolumeTypesAndShowLimit() {
  controller.setWarningBannersInOrder([
    testWarningBanners[0].tagName,
  ]);
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0].tagName,
  ]);

  // Set the allowed volume types for the warning banners.
  testWarningBanners[0].setAllowedVolumeTypes([driveAllowedVolumeType]);

  // Set the showLimit and allowed volume types for the educational banner.
  testEducationalBanners[0].setShowLimit(2);
  testEducationalBanners[0].setAllowedVolumeTypes([downloadsAllowedVolumeType]);

  // The first reconciliation should increment the counter and append to DOM.
  await controller.initialize();
  assertBannerVisible(testEducationalBanners[0]);

  // Change the directory to Drive.
  changeCurrentVolume(VolumeManagerCommon.VolumeType.DRIVE, null);
  await controller.reconcile();
  assertBannerVisible(testWarningBanners[0]);

  // Start a new Files app session (clearing the container emulates this).
  // Change the directory back Downloads.
  bannerContainer.textContent = '';
  changeCurrentVolume(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
  await controller.reconcile();
  assertBannerVisible(testEducationalBanners[0]);

  // Change back to Drive, warning banner should still be showing.
  changeCurrentVolume(VolumeManagerCommon.VolumeType.DRIVE, null);
  await controller.reconcile();
  assertBannerVisible(testWarningBanners[0]);

  // Emulate starting a new Files app session and change back to Downloads
  // volume. This time the educational banner should no longer be showing as it
  // has exceeded it's show limit.
  bannerContainer.textContent = '';
  changeCurrentVolume(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
  await controller.reconcile();
  assertNoVisibleBanners();
}
