// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!HTMLElement}
 */
let container;

/**
 * @type {!DirectoryModel}
 */
let directoryModel;

/**
 * @type {!FakeEntry}
 */
let recentEntry;

/**
 * @type {!EntryList}
 */
let myFilesEntry;

/**
 * @type {!FileTypeFiltersController}
 */
let fileTypeFiltersController;

function setUp() {
  // Mock loadTimeData strings.
  window.loadTimeData.data = {
    MEDIA_VIEW_AUDIO_ROOT_LABEL: 'Audio',
    MEDIA_VIEW_IMAGES_ROOT_LABEL: 'Images',
    MEDIA_VIEW_VIDEOS_ROOT_LABEL: 'Videos',
  };

  class MockDirectoryModel extends cr.EventTarget {
    constructor() {
      super();

      this.currentDirEntry = null;
      window.isRescanCalled = false;
    }

    rescan(refresh) {
      window.isRescanCalled = true;
    }

    changeDirectoryEntry(dirEntry) {
      // Change the directory model's current directory to |dirEntry|.
      const previousDirEntry = this.currentDirEntry;
      this.currentDirEntry = dirEntry;

      // Emit 'directory-changed' event synchronously to simplify testing.
      const event = new Event('directory-changed');
      event.previousDirEntry = previousDirEntry;
      event.newDirEntry = this.currentDirEntry;
      this.dispatchEvent(event);
    }

    static create() {
      const model = /** @type {!Object} */ (new MockDirectoryModel());
      return /** @type {!DirectoryModel} */ (model);
    }
  }

  // Create FileTypeFiltersController instance with dependencies.
  container = /** @type {!HTMLInputElement} */ (document.createElement('div'));
  directoryModel = MockDirectoryModel.create();
  recentEntry = new FakeEntryImpl(
      'Recent', VolumeManagerCommon.RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE);
  fileTypeFiltersController =
      new FileTypeFiltersController(container, directoryModel, recentEntry);

  // Create a directory entry which is not Recents to simulate directory change.
  myFilesEntry =
      new EntryList('My Files', VolumeManagerCommon.RootType.MY_FILES);
}

/**
 * Tests that creating FileTypeFiltersController generates three buttons in the
 * given container element.
 */
function testCreatedButtonLabels() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  assertEquals(buttons[0].textContent, 'Audio');
  assertEquals(buttons[1].textContent, 'Images');
  assertEquals(buttons[2].textContent, 'Videos');
}

/**
 * Tests that initial states of all buttons inside container are inactive.
 */
function testButtonInitialActiveState() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[0].classList.contains('active'));
}

/**
 * Tests that click events toggle button state (inactive -> active -> inactive).
 */
function testButtonToggleState() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  assertFalse(buttons[0].classList.contains('active'));
  buttons[0].click();
  assertTrue(buttons[0].classList.contains('active'));
  buttons[0].click();
  assertFalse(buttons[0].classList.contains('active'));
}

/**
 * Tests that only one button can be inactive.
 * If button_1 is clicked then button_0 is active, button_0 becomes inactive and
 * button_1 becomes active.
 */
function testOnlyOneButtonCanActive() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  assertFalse(buttons[0].classList.contains('active'));
  buttons[0].click();
  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));

  buttons[1].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertTrue(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));

  buttons[2].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertTrue(buttons[2].classList.contains('active'));
}

/**
 * Tests that container element is visible only when the current directory is
 * Recents view.
 */
function testContainerIsShownOnlyInRecents() {
  container.hidden = true;
  directoryModel.changeDirectoryEntry(recentEntry);
  assertFalse(container.hidden);
  directoryModel.changeDirectoryEntry(myFilesEntry);
  assertTrue(container.hidden);
}

/**
 * Tests that button's active state is reset to inactive when the user leaves
 * Recents view.
 */
function testActiveButtonIsResetOnLeavingRecents() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  directoryModel.changeDirectoryEntry(recentEntry);

  buttons[0].click();
  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));

  directoryModel.changeDirectoryEntry(myFilesEntry);
  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
}

/**
 * Tests that the active state of each button is reflected to the Recent entry's
 * recentFileType property, and DirectoryModel.rescan() is called after the
 * Recent entry's property is modified.
 */
function testAppliedFilters() {
  const buttons = container.children;
  assertEquals(buttons.length, 3);

  directoryModel.changeDirectoryEntry(recentEntry);

  buttons[0].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.AUDIO);
  assertTrue(window.isRescanCalled);
  window.isRescanCalled = false;

  buttons[1].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.IMAGE);
  assertTrue(window.isRescanCalled);
  window.isRescanCalled = false;

  buttons[2].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.VIDEO);
  assertTrue(window.isRescanCalled);
  window.isRescanCalled = false;

  buttons[2].click();
  assertEquals(
      recentEntry.recentFileType, chrome.fileManagerPrivate.RecentFileType.ALL);
  assertTrue(window.isRescanCalled);
  window.isRescanCalled = false;
}
