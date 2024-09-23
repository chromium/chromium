// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {Crostini} from '../../background/js/crostini.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {installMockChrome, MockChromeFileManagerPrivateDirectoryChanged} from '../../common/js/mock_chrome.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {storage} from '../../common/js/storage.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {getRootTypeFromVolumeType, RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {DialogType} from '../../state/state.js';

import {BannerController} from './banner_controller.js';
import type {DirectoryModel} from './directory_model.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import {type AllowedVolumeOrType, Banner, BANNER_INFINITE_TIME, BannerEvent, type MinDiskThreshold} from './ui/banners/types.js';

let controller: BannerController;

let directoryModel: DirectoryModel;

let bannerContainer: Element;

let mockChromeFileManagerPrivate: MockChromeFileManagerPrivateDirectoryChanged;

let volumeManagerGetVolumeInfoType: VolumeInfo;

let mockDate: {setDate: (date: number) => void, restoreDate: () => void};

let mockFileSystem: MockFileSystem;

let mockEntry: Entry;

interface TestBanner {
  setAllowedVolumes: (allowedBannerTypes: AllowedVolumeOrType[]) => void;
  setShowLimit: (limit: number) => void;
  setDiskThreshold: (threshold: MinDiskThreshold) => void;
  setHideAfterDismissedDurationSeconds: (seconds: number) => void;
  setTimeLimit: (limit: number) => void;
  getFilteredContext: () => void;
  reset: () => void;
  tagName: string;
}

/**
 * Maintains the registered web components for testing the warning banners.
 */
const testWarningBanners: TestBanner[] = [];

/**
 * Maintains the registered web components for testing the educational banners.
 */
const testEducationalBanners: TestBanner[] = [];

/**
 * Number of banners to create on startup.
 */
const BANNERS_COUNT = 5;

const downloadsAllowedVolumeType: AllowedVolumeOrType = {
  type: VolumeType.DOWNLOADS,
};

const driveAllowedVolumeType: AllowedVolumeOrType = {
  type: VolumeType.DRIVE,
};

const androidFilesAllowedVolumeType: AllowedVolumeOrType = {
  type: VolumeType.ANDROID_FILES,
};

/**
 * Returns an object with helper methods to manipulate a test banner.
 */
function createTestBanner(tagName: string) {
  let allowedVolumes: AllowedVolumeOrType[] = [];
  let showLimit: number|undefined;
  let diskThreshold: MinDiskThreshold|undefined;
  let hideAfterDismissDurationSeconds: number;
  let timeLimitSeconds: number;
  let context: Object;

  class FakeBanner extends Banner {
    override allowedVolumes() {
      return allowedVolumes;
    }

    override showLimit() {
      return showLimit;
    }

    override diskThreshold() {
      return diskThreshold;
    }

    override timeLimit() {
      return timeLimitSeconds;
    }

    override hideAfterDismissedDurationSeconds() {
      return hideAfterDismissDurationSeconds;
    }

    override onFilteredContext(filteredContext: Object) {
      context = filteredContext;
    }
  }

  customElements.define(tagName, FakeBanner);

  return {
    setAllowedVolumes: (types: AllowedVolumeOrType[]) => {
      allowedVolumes = types;
    },
    reset: () => {
      allowedVolumes = [];
      showLimit = undefined;
      diskThreshold = undefined;
    },
    setShowLimit: (limit: number) => {
      showLimit = limit;
    },
    setDiskThreshold: (threshold: MinDiskThreshold) => {
      diskThreshold = threshold;
    },
    setHideAfterDismissedDurationSeconds: (duration: number) => {
      hideAfterDismissDurationSeconds = duration;
    },
    setTimeLimit: (seconds: number) => {
      timeLimitSeconds = seconds;
    },
    getFilteredContext: () => {
      return context;
    },
    // Element.tagName returns uppercase.
    tagName: tagName.toUpperCase(),
  };
}

/**
 * Helper method to use with waitUntil to assert that only one banner is
 * visible.
 */
function isOnlyBannerVisible(banner: TestBanner) {
  return function() {
    const visibleBanner = document.querySelector(banner.tagName);

    if (!visibleBanner) {
      return false;
    }

    if (visibleBanner.hasAttribute('hidden') ||
        visibleBanner.getAttribute('aria-hidden') === 'true') {
      return false;
    }

    for (let i = 0; i < bannerContainer.children.length; i++) {
      const element = bannerContainer.children.item(i);
      if (element === visibleBanner) {
        continue;
      }
      if (element &&
          (!element.hasAttribute('hidden') ||
           element.getAttribute('aria-hidden') === 'false')) {
        return false;
      }
    }
    return true;
  };
}

/**
 * Helper method to use with waitUntil to assert that all banners are hidden.
 */
function isAllBannersHidden() {
  for (let i = 0; i < bannerContainer.children.length; i++) {
    const element = bannerContainer.children.item(i)!;
    if (!element.hasAttribute('hidden') ||
        element.getAttribute('aria-hidden') === 'false') {
      return false;
    }
  }
  return true;
}

/**
 * Changes the global directory to |newVolume|.
 */
function changeCurrentVolume(
    volumeType: VolumeType|null, volumeId: string|null = null,
    rootType: RootType|null = null) {
  directoryModel.getCurrentVolumeInfo = function(): any {
    // Certain directory roots return null (USB drive root).
    if (!volumeType) {
      return null;
    }
    return {
      volumeType,
      volumeId,
    } as any as VolumeInfo;
  };

  // Infer the root type from the volume type unless explicitly defined.
  directoryModel.getCurrentRootType = function(): any {
    if (rootType) {
      return rootType;
    }
    if (!volumeType) {
      return null;
    }
    return getRootTypeFromVolumeType(volumeType);
  };

  directoryModel.getCurrentDirEntry = function(): any {
    const rootType = directoryModel.getCurrentRootType();
    return rootType ? mockEntry : null;
  };

  directoryModel.dispatchEvent(new CustomEvent('directory-changed'));
}

/**
 * Update the current volume size stats that are returned by
 * chrome.fileManagerPrivate.getSizeStats.
 */
function changeCurrentVolumeDiskSpace(
    newSizeStats: chrome.fileManagerPrivate.MountPointSizeStats|null,
    dispatchEvent = true) {
  const currentVolume = directoryModel.getCurrentVolumeInfo();
  if (!currentVolume || !currentVolume.volumeId) {
    return;
  }

  if (currentVolume.volumeType === VolumeType.DRIVE) {
    if (!newSizeStats) {
      mockChromeFileManagerPrivate.unsetDriveQuotaMetadata();
    } else {
      mockChromeFileManagerPrivate.setDriveQuotaMetadata({
        totalBytes: newSizeStats.totalSize,
        usedBytes: newSizeStats.totalSize - newSizeStats.remainingSize,
        organizationLimitExceeded: false,
        organizationName: 'Test Org',
        userType: chrome.fileManagerPrivate.UserType.UNMANAGED,
      });
    }
  } else {
    if (!newSizeStats) {
      mockChromeFileManagerPrivate.unsetVolumeSizeStats(currentVolume.volumeId);
    } else {
      mockChromeFileManagerPrivate.setVolumeSizeStats(
          currentVolume.volumeId, newSizeStats);
    }
  }

  volumeManagerGetVolumeInfoType = currentVolume;
  if (dispatchEvent) {
    mockChromeFileManagerPrivate.dispatchOnDirectoryChanged();
  }
}

/**
 * Sends a DISMISS event to the supplied banner. This synthetic event is
 * wired up to the event handler set in the BannerController.
 */
function clickDismissButton(banner: TestBanner) {
  const visibleBanner = bannerContainer.querySelector<Banner>(banner.tagName)!;
  visibleBanner.dispatchEvent(new CustomEvent(
      BannerEvent.BANNER_DISMISSED,
      {bubbles: true, composed: true, detail: {banner: visibleBanner}}));
}

/**
 * Sends a DISMISS_FOREVER event to the supplied banner. This synthetic event is
 * wired up to the event handler set in the BannerController.
 */
function clickDismissForeverButton(banner: TestBanner) {
  const visibleBanner = bannerContainer.querySelector<Banner>(banner.tagName)!;
  visibleBanner.dispatchEvent(new CustomEvent(
      BannerEvent.BANNER_DISMISSED_FOREVER,
      {bubbles: true, composed: true, detail: {banner: visibleBanner}}));
}

/**
 * Mocks the Date.now() function to enable us to not have to wait for a
 * specified duration. Returns 2 functions, one to set the Date.now() and one
 * to restore the Date.now() to it's existing functionality.
 */
function mockDateNow() {
  const dateNowRestore = Date.now;
  const setDate = (seconds: number) => {
    // Date.now works in milliseconds, convert seconds appropriately.
    Date.now = () => seconds * 1000;
  };
  const restoreDate = () => {
    Date.now = dateNowRestore;
  };
  return {setDate, restoreDate};
}

/**
 * Imitates a new Files app session. The Banner controller looks to see if the
 * banner has been connected to the DOM and so clearing the DOM should
 * sufficiently demonstrate this.
 */
async function startNewFilesAppSession() {
  bannerContainer.textContent = '';
  await controller.reconcile();
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

  installMockChrome({
    runtime: {
      lastError: undefined,
    },
  });

  mockChromeFileManagerPrivate =
      new MockChromeFileManagerPrivateDirectoryChanged();

  mockFileSystem = new MockFileSystem('volumeId');
  mockFileSystem.populate(['/']);
  mockEntry = mockFileSystem.entries['/']!;

  directoryModel = createFakeDirectoryModel();
  const volumeManager = {
    getVolumeInfo: (_: FileSystemEntry) => {
      return volumeManagerGetVolumeInfoType;
    },
  } as VolumeManager;
  const crostini = {} as Crostini;
  controller = new BannerController(
      directoryModel, volumeManager, crostini, DialogType.SELECT_SAVEAS_FILE);
  controller.disableBannerLoadingForTesting();

  mockDate = mockDateNow();

  // Ensure local storage is cleared between each test.
  storage.local.clear();
}

export function tearDown() {
  bannerContainer.textContent = '';

  for (let i = 0; i < BANNERS_COUNT; i++) {
    testWarningBanners[i]!.reset();
    testEducationalBanners[i]!.reset();
  }

  mockDate.restoreDate();
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
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  // Set the allowed volume type to be the DOWNLOADS directory.
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test a banner with lower priority can be shown if the higher priority banner
 * does not match the current volume type.
 */
export async function testNextMatchingBannerShows() {
  // Add 1 warning and 1 educational banner to the controller.
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  // Only set the educational banner to have allowed type of DOWNLOADS.
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));
}

/**
 * Test banners of the same types are prioritised by the order they are set.
 */
export async function testBannersArePrioritisedByIndexOrder() {
  // Add 2 warning banners.
  controller.setWarningBannersInOrder([
    testWarningBanners[0]!.tagName,
    testWarningBanners[1]!.tagName,
  ]);

  // Set the banner at index 1 to be shown.
  testWarningBanners[1]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[1]!));
}

/**
 * Test banners of the same types are prioritised by the order they are set.
 */
export async function testBannersAreHiddenOnVolumeChange() {
  // Add 2 warning banners and 1 educational banner.
  controller.setWarningBannersInOrder([
    testWarningBanners[0]!.tagName,
    testWarningBanners[1]!.tagName,
  ]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  // Set the following banners to show:
  //  First Warning Banner shows on Downloads.
  //  Second Warning Banner shows on Drive.
  //  First Educational Banner shows on Android Files.
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[1]!.setAllowedVolumes([driveAllowedVolumeType]);
  testEducationalBanners[0]!.setAllowedVolumes([androidFilesAllowedVolumeType]);

  // Verify for Downloads the first banner shows.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Change volume to Drives and verify the second warning banner is showing.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[1]!));

  // Change volume to Android files and verify the educational banner is shown.
  changeCurrentVolume(VolumeType.ANDROID_FILES);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));
}

/**
 * Test that a null VolumeInfo hides all the banners.
 */
export async function testNullVolumeInfoClearsBanners() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // Verify for Downloads the warning banner is shown.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Change current volume to a null volume type and assert no banner is shown.
  changeCurrentVolume(/* volumeType */ null);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that banners that have hit their show limit are not shown again.
 */
export async function testBannersChangeAfterShowLimitReached() {
  // Add 2 educational banners.
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0]!.tagName,
    testEducationalBanners[1]!.tagName,
  ]);

  // Set the showLimit for the educational banners.
  testEducationalBanners[0]!.setShowLimit(1);
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[1]!.setShowLimit(3);
  testEducationalBanners[1]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // The first reconciliation should increment the counter and append to DOM.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  await startNewFilesAppSession();

  // After a new Files app session has started, the previous counter has
  // exceeded it's show limit, assert the next priority banner is shown.
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[1]!));
}

/**
 * Test that the show limit is increased only on a per app session.
 */
export async function testChangingVolumesDoesntIncreaseShowTimes() {
  // Add 1 educational banner.
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  const bannerShowLimit = 3;
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[0]!.setShowLimit(bannerShowLimit);

  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Change directory one more times than the show limit to verify the show
  // limit doesn't increase.
  for (let i = 0; i < bannerShowLimit + 1; i++) {
    // Change directory and verify no banner shown.
    changeCurrentVolume(VolumeType.DRIVE);
    await waitUntil(isAllBannersHidden);

    // Change back to DOWNLOADS and verify banner has been shown.
    changeCurrentVolume(VolumeType.DOWNLOADS);
    await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));
  }
}

/**
 * Test that multiple banners with different allowedVolumes and show limits
 * are show at the right stages. This also asserts that banners that don't
 * implement showLimit still are shown as expected.
 */
export async function testMultipleBannersAllowedVolumesAndShowLimit() {
  controller.setWarningBannersInOrder([
    testWarningBanners[0]!.tagName,
  ]);
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0]!.tagName,
  ]);

  // Set the allowed volume types for the warning banners.
  testWarningBanners[0]!.setAllowedVolumes([driveAllowedVolumeType]);

  // Set the showLimit and allowed volume types for the educational banner.
  testEducationalBanners[0]!.setShowLimit(2);
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // The first reconciliation should increment the counter and append to DOM.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Change the directory to Drive.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  await startNewFilesAppSession();

  // Change the directory back Downloads.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Change back to Drive, warning banner should still be showing.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  await startNewFilesAppSession();

  // Educational banner should no longer be showing as it has exceeded it's show
  // limit.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that when chrome.fileManagerPrivate.getSizeStats returns null,
 * banners relying on specific size are not shown.
 */
export async function testNullGetSizeStatsDoesntTriggerThreshold() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });

  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace(null);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that setting the remaining size coming back from getSizeStats triggers
 * the banner to display.
 */
export async function testVolumeSizeChangeShowsBanner() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });

  // When a volume is first navigated to it retrieves the size of the disk at
  // first. Ensure the first size it retrieves is above the threshold.
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });

  await controller.initialize();
  await waitUntil(isAllBannersHidden);

  // Change remaining size in the current volume to 512 KB (less than 1 GB)
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 512 * 1024 * 1024,    // 512 KB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test that when a volume size changes multiple events in sequence are
 * debounced to avoid the scenario when lots of small size files cause many
 * events to be emitted that will lock up the UI thread.
 */
export async function testVolumeSizeChangeIsDebounced() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });

  // When a volume is first navigated to it retrieves the size of the disk at
  // first. Ensure the first size it retrieves is above the threshold.
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });

  await controller.initialize();
  await waitUntil(isAllBannersHidden);

  // Set the date to 1s, any time from [1s, 6s] will cause the debouncing to
  // kick in and therefore no banner will be visible.
  mockDate.setDate(1);

  // Change remaining size in the current volume to 512 KB (less than 1 GB)
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 512 * 1024 * 1024,    // 512 KB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Set the date to 3s (still within the debounce interval) and change the size
  // to cause the banner to not show. Expect this to not happen until the time
  // is set to >6s which is outside the debounce window. Update the current
  // volume multiple times ensuring the banner never changes.
  mockDate.setDate(3);
  for (let i = 0; i < 10; i++) {
    changeCurrentVolumeDiskSpace({
      totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
      remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
    });
    await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  }

  // Once the date is set to a time outside the debounce interval, the banner
  // should hide.
  mockDate.setDate(7);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that when the remaining space goes below the threshold, the banner is
 * show and if enough free space is reclaimed the banner is hidden.
 */
export async function testVolumeSizeBelowShowsBannerAndAboveHidesBanner() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });

  // When a volume is first navigated to it retrieves the size of the disk at
  // first. Ensure the first size it retrieves is above the threshold.
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });

  await controller.initialize();
  await waitUntil(isAllBannersHidden);

  // Verify banner is shown when disk space goes below threshold.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 512 * 1024 * 1024,    // 512 KB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Verify banner is hidden when free space goes above minSize.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 2 * 1024 * 1024 * 1024,  // 2 GB
  });
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that multiple banners (with different volume size observers; minRatio
 * and minSize) can co-exist and will be shown at appropriate times.
 */
export async function testTwoVolumeBannersShowOnWatchedVolumeTypes() {
  controller.setWarningBannersInOrder([
    testWarningBanners[0]!.tagName,
    testWarningBanners[1]!.tagName,
  ]);

  // Banner should show on Downloads when volume goes below 1GB remaining size.
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });

  // Banner should show on Drive when volume goes below 20% remaining free
  // space.
  testWarningBanners[1]!.setAllowedVolumes([driveAllowedVolumeType]);
  testWarningBanners[1]!.setDiskThreshold({
    type: RootType.DRIVE,
    minRatio: 0.2,
  });

  // When a volume is first navigated to it retrieves the size of the disk at
  // first. Ensure the first size it retrieves is above the threshold.
  changeCurrentVolume(VolumeType.DRIVE, 'drive-hash');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });

  await controller.initialize();
  await waitUntil(isAllBannersHidden);

  // Verify well below threshold, banner is triggered for minSize.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 512 * 1024 * 1024,    // 512 KB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Verify equal to threshold, banner is triggered for minSize.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Change volume to Drive and ensure banner is not shown.
  changeCurrentVolume(VolumeType.DRIVE, 'drive-hash');
  await waitUntil(isAllBannersHidden);

  // Verify well below threshold, banner is triggered for minRatio.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 2 * 1024 * 1024 * 1024,  // 2 GB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[1]!));

  // Verify equal to threshold, banner is triggered for minRatio.
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 4 * 1024 * 1024 * 1024,  // 4 GB
  });
  await waitUntil(isOnlyBannerVisible(testWarningBanners[1]!));
}

/**
 * Test that if a volume is changed mid way through a volume size request, the
 * banner is not shown, but the cache is maintained such that navigating back
 * to the volume causes the banner to show.
 */
export async function testChangingDirectoryMidSizeUpdateHidesBanner() {
  controller.setWarningBannersInOrder(
      [testWarningBanners[0]!.tagName, testWarningBanners[1]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setDiskThreshold({
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  });
  testWarningBanners[1]!.setAllowedVolumes([driveAllowedVolumeType]);
  testWarningBanners[1]!.setDiskThreshold({
    type: RootType.DRIVE,
    minRatio: 0.2,
  });

  // When a volume is first navigated to it retrieves the size of the disk at
  // first. Ensure the first size it retrieves is above the threshold.
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  changeCurrentVolumeDiskSpace({
    totalSize: 20 * 1024 * 1024 * 1024,      // 20 GB
    remainingSize: 20 * 1024 * 1024 * 1024,  // 10 GB
  });

  // Change volume to downloads to ensure the appropriate event listener is
  // attached to the volume.
  await controller.initialize();
  await waitUntil(isAllBannersHidden);

  // Change the underlying disk space to breach the threshold, but don't
  // dispatch an onDirectoryChanged event.
  changeCurrentVolumeDiskSpace(
      {
        totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
        remainingSize: 512 * 1024 * 1024,    // 512 KB
      },
      /* dispatchEvent */ false);

  // Change the current volume and dispatch an onDirectoryChanged event, this
  // emulates a user changing the directory mid volume size request.
  changeCurrentVolume(VolumeType.DRIVE, 'drive-hash');
  mockChromeFileManagerPrivate.dispatchOnDirectoryChanged();
  await waitUntil(isAllBannersHidden);

  // Change the volume back to the downloads directory and assert the directory
  // size causes the banner to be shown.
  changeCurrentVolume(VolumeType.DOWNLOADS, 'downloads');
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test the dismiss button hides the banner appropriately.
 */
export async function testDismissHidesBanner() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // Set the hidden duration to 999999 seconds to ensure it doesn't reappear.
  testWarningBanners[0]!.setHideAfterDismissedDurationSeconds(999999);

  // Verify for Downloads the warning banner is shown.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Click the dismiss button and ensure the banner is hidden.
  clickDismissButton(testWarningBanners[0]!);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that the duration set for the banner to be hidden for is respected.
 */
export async function testDismissedBannerShowsAfterDuration() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setHideAfterDismissedDurationSeconds(15);

  // Verify for Downloads the warning banner is shown.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Move the clock to 10s and click the dismiss button. This should have the
  // banner be hidden for the duration [10s, 25s].
  mockDate.setDate(10);
  clickDismissButton(testWarningBanners[0]!);
  await waitUntil(isAllBannersHidden);

  // Move the clock to 1 second after the banner duration has expired (banner
  // expires at 25s, so 26s the banner should appear again). Change the
  // directory to the same place to kick off a reconciliation.
  mockDate.setDate(26);
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test that a navigating around a Files app session after a banner has been
 * shown does not hide the banner if the showLimit has been reached, only the
 * next Files app session should hide the banner.
 */
export async function testBannerContinuesShowingThroughoutAppSession() {
  // Add warning banner.
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);

  // Set the showLimit for the warning banner.
  testWarningBanners[0]!.setShowLimit(2);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // The first reconciliation should increment the counter and append to DOM.
  // Show counter should equal 1.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  await startNewFilesAppSession();

  // Navigating to Downloads should increment the counter and append to the DOM.
  // Show counter should equal 2.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Changing to another volume should hide the banner.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isAllBannersHidden);

  // Navigating back to Downloads should still have the banner shown. The same
  // Files app session is still active (even though the count is 2 which is the
  // showLimit).
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  await startNewFilesAppSession();

  // This is the 3rd Files app session, when navigating to Downloads the banner
  // should not be visible, the counter should now be 3 exceeding the showLimit
  // of 2.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that if multiple banners are appropriately hidden when flipping between
 * being the highest priority banner to be shown.
 */
export async function testMultipleWinningBannersOnlyTopPriorityShown() {
  // Add 2 banners.
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  // Set the educational banner to show on both Downloads and Drive and the
  // warning banner to show only on Downloads.
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[0]!.setAllowedVolumes([
    downloadsAllowedVolumeType,
    driveAllowedVolumeType,
  ]);

  // Changing to the Downloads directory should show the warning banner only.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Changing to Drive directory should show the educational banner only.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // If we change back to the Downloads directory the warning banner should be
  // the only banner shown. This should explicitly hide the educational banner
  // even though shouldShowBanner returns true, as the warning banner is higher
  // priority.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test that a banner with timeLimit set hides after the defined time limit.
 */
export async function testTimeLimitReachedHidesBanner() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setTimeLimit(60);

  mockDate.setDate(1);

  // Verify that the banner is initially shown correctly then manually invoke
  // the updateTimeLimit. The time limit is updated after the first interval so
  // we can expect the time limit to be
  // 1 + DURATION_BETWEEN_TIME_LIMIT_UPDATES_MS.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  const banner =
      document.querySelector<Banner>(testWarningBanners[0]!.tagName)!;
  await controller.updateTimeLimit(banner);

  // Move the clock to 30 seconds and update the time limit shown. Given the
  // time limit is updated after the fact the time shown is at 10s now, so we
  // expect this to update to 40s (still below the 60s time limit).
  mockDate.setDate(30);
  await controller.updateTimeLimit(banner);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Move the clock to 61 and update the time limit shown. The time limit should
  // be recorded as 40s and this should take it to 61s which is beyond the time
  // limit specified and thus should be hidden.
  mockDate.setDate(61);
  await controller.updateTimeLimit(banner);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that the constant Banner.INFINITE_TIME allows the banner to display for
 * a super long time.
 */
export async function testInfiniteTimeLimitWorks() {
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setTimeLimit(BANNER_INFINITE_TIME);

  mockDate.setDate(1);

  // Verify that the banner is initially shown correctly then manually invoke
  // the updateTimeLimit.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  const banner =
      document.querySelector<Banner>(testWarningBanners[0]!.tagName)!;
  await controller.updateTimeLimit(banner);

  // Move the clock to 9999999 seconds and verify the banner is still visible.
  mockDate.setDate(9999999);
  await controller.updateTimeLimit(banner);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
}

/**
 * Test the dismiss forever correctly hides the banner during the current Files
 * app session.
 */
export async function testEducationalBannerDismissedForever() {
  // Add 1 educational banner.
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);

  // Set the educational banner to downloads volume type and show limit to 10.
  testEducationalBanners[0]!.setShowLimit(10);
  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // Changing to the Downloads directory should show the educational banner
  // only.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Click the dismiss forever button which should set the times shown to past
  // the maximum number of times to show. This banner should never show again.
  clickDismissForeverButton(testEducationalBanners[0]!);

  // Navigate to Drive and back to Downloads and expect the banner to be hidden.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isAllBannersHidden);
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that banners are reconciled after every dismiss click that is made.
 */
export async function testBannersAreUpdatedOnDismissClick() {
  // Add 3 educational banner and 1 warning banner.
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0]!.tagName,
    testEducationalBanners[1]!.tagName,
    testEducationalBanners[2]!.tagName,
  ]);
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);

  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[1]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[2]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // Verify that the warning banner is shown first.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Dismiss the warning banner and assert the highest priority educational
  // banner is shown (without navigating directories).
  clickDismissButton(testWarningBanners[0]!);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Dismiss the educational banner and assert the next highest priority
  // educational banner is shown (without navigating directories).
  clickDismissForeverButton(testEducationalBanners[0]!);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[1]!));

  // Dismiss the educational banner and assert the next highest priority
  // educational banner is shown (without navigating directories).
  clickDismissForeverButton(testEducationalBanners[1]!);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[2]!));

  // Dismiss the last educational banner and ensure no banners are shown (again
  // without navigating directories).
  clickDismissForeverButton(testEducationalBanners[2]!);
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that custom filters can be attached to banners in the event specific
 * logic needs to be ran for individual banners.
 */
export async function testCustomFiltersCanBeAttached() {
  // Add 1 educational banner and 1 warning banner.
  controller.setEducationalBannersInOrder([testEducationalBanners[0]!.tagName]);
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);

  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  let educationalBannerCustomFilterResponse = true;
  let warningBannerCustomFilterResponse = true;

  controller.registerCustomBannerFilter(testEducationalBanners[0]!.tagName, {
    shouldShow: () => educationalBannerCustomFilterResponse,
    context: () => ({}),
  });
  controller.registerCustomBannerFilter(testWarningBanners[0]!.tagName, {
    shouldShow: () => warningBannerCustomFilterResponse,
    context: () => ({}),
  });

  // Verify that the warning banner is shown first.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Verify that explicitly making the custom filter return true reconciles
  // the banners and does not show the warning banner.
  warningBannerCustomFilterResponse = false;
  await startNewFilesAppSession();
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Ensure no banners are shown when both custom filters return true.
  educationalBannerCustomFilterResponse = false;
  await startNewFilesAppSession();
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that custom filters interact with other configuration elements.
 */
export async function testCustomFiltersInteract() {
  // Add 2 educational banners and 1 warning banner.
  controller.setEducationalBannersInOrder([
    testEducationalBanners[0]!.tagName,
    testEducationalBanners[1]!.tagName,
  ]);
  controller.setWarningBannersInOrder([
    testWarningBanners[0]!.tagName,
  ]);

  testEducationalBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);
  testEducationalBanners[0]!.setShowLimit(2);
  testEducationalBanners[1]!.setAllowedVolumes([driveAllowedVolumeType]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  let warningBannerCustomFilterResponse = true;

  controller.registerCustomBannerFilter(testWarningBanners[0]!.tagName, {
    shouldShow: () => warningBannerCustomFilterResponse,
    context: () => ({}),
  });

  // Verify that the warning banner is shown first.
  await controller.initialize();

  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));

  // Changing directory to Drive should show the lowest priority educational
  // banner.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[1]!));

  // Making the custom filter return false and navigating back to the Downloads
  // directory should show the highest priority educational banner.
  warningBannerCustomFilterResponse = false;
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // Starting a fresh Files app twice should hide the educational banner as it
  // has hit the show limit. This should nicely interact with the custom filter
  // (i.e. no banner should show).
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[1]!));

  // Reset the current volume to Downloads so the new Files app session is
  // already navigated to Downloads.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  // This should be the final Files app session, should not show on the next
  // session.
  await startNewFilesAppSession();
  await waitUntil(isOnlyBannerVisible(testEducationalBanners[0]!));

  await startNewFilesAppSession();
  await waitUntil(isAllBannersHidden);
}

/**
 * Test that custom banners receive their requested context when the filter
 * function is executed.
 */
export async function testCustomContextIsReceivedByBanner() {
  // Add 1 warning banner.
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  const warningBannerCustomFilterResponse = true;
  const warningBannerCustomContext: {[key: string]: string} = {
    'fake-key': 'test-fake-value',
  };

  controller.registerCustomBannerFilter(testWarningBanners[0]!.tagName, {
    shouldShow: () => warningBannerCustomFilterResponse,
    context: () => warningBannerCustomContext,
  });

  // Verify that the warning banner is shown first.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  assertDeepEquals(
      testWarningBanners[0]!.getFilteredContext(), warningBannerCustomContext);

  // Change to Drive volume to ensure changing directory re-verifies the filter.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isAllBannersHidden);

  // Update the context of the filter and change back to Downloads. Expect that
  // the context is retrieved every time to ensure fresh data.
  warningBannerCustomContext['another-fake-key'] = 'another-fake-value';
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  assertDeepEquals(
      testWarningBanners[0]!.getFilteredContext(), warningBannerCustomContext);
}

/**
 * Test that if multiple filters are registered to a single banner, only the
 * winning filter has their associated context passed to the banner.
 */
export async function testWinningFilterContextIsPassed() {
  // Add 1 warning banner.
  controller.setWarningBannersInOrder([testWarningBanners[0]!.tagName]);
  testWarningBanners[0]!.setAllowedVolumes([downloadsAllowedVolumeType]);

  // Setup 2 custom filters on the warning banner.
  let warningBannerCustomFilter1Response = true;
  const warningBannerCustomFilter2Response = true;
  const warningBannerCustomContext1 = {
    'fake-key-1': 'test-fake-value-1',
  };
  const warningBannerCustomContext2 = {
    'fake-key-2': 'test-fake-value-2',
  };
  controller.registerCustomBannerFilter(testWarningBanners[0]!.tagName, {
    shouldShow: () => warningBannerCustomFilter1Response,
    context: () => warningBannerCustomContext1,
  });
  controller.registerCustomBannerFilter(testWarningBanners[0]!.tagName, {
    shouldShow: () => warningBannerCustomFilter2Response,
    context: () => warningBannerCustomContext2,
  });

  // Verify that even though both filters allow the banner to display, only the
  // context for the first registered filter is passed to the banner.
  await controller.initialize();
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  assertDeepEquals(
      testWarningBanners[0]!.getFilteredContext(), warningBannerCustomContext1);

  // Change directory to Drive to enable a change back to re-verify the custom
  // filters.
  changeCurrentVolume(VolumeType.DRIVE);
  await waitUntil(isAllBannersHidden);

  // Set the first registered filter to hide the banner.
  warningBannerCustomFilter1Response = false;

  // Change back to Downloads and ensure the second registered filter shows the
  // banner and passes the context setup.
  changeCurrentVolume(VolumeType.DOWNLOADS);
  await waitUntil(isOnlyBannerVisible(testWarningBanners[0]!));
  assertDeepEquals(
      testWarningBanners[0]!.getFilteredContext(), warningBannerCustomContext2);
}
