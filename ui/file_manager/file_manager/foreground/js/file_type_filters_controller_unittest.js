// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {EntryList, FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {metrics} from '../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';
import {FileTypeFiltersController} from './file_type_filters_controller.js';
import {A11yAnnounce} from './ui/a11y_announce.js';

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

/**
 * Mock metrics.
 * @param {string} name
 * @param {*} value
 * @param {Array<*>|number=} opt_validValues
 */
metrics.recordEnum = function(name, value, opt_validValues) {};

const TOTAL_FILTER_BUTTON_COUNT = 5;

export function setUp() {
  class MockDirectoryModel extends EventTarget {
    constructor() {
      super();

      this.currentDirEntry = null;
      window.isScanCalled = false;
    }

    clearCurrentDirAndScan() {
      window.isScanCalled = true;
    }

    changeDirectoryEntry(dirEntry) {
      // Change the directory model's current directory to |dirEntry|.
      const previousDirEntry = this.currentDirEntry;
      this.currentDirEntry = dirEntry;

      // Emit 'directory-changed' event synchronously to simplify testing.
      const event = new DirectoryChangeEvent('directory-changed');
      event.previousDirEntry = previousDirEntry;
      event.newDirEntry = this.currentDirEntry;
      this.dispatchEvent(event);
    }

    static create() {
      const model = /** @type {!Object} */ (new MockDirectoryModel());
      return /** @type {!DirectoryModel} */ (model);
    }
  }

  const mockA11y = /** @type {!A11yAnnounce} */ ({
    speakA11yMessage: () => {},
  });

  // Create FileTypeFiltersController instance with dependencies.
  container = /** @type {!HTMLInputElement} */ (document.createElement('div'));
  directoryModel = MockDirectoryModel.create();
  recentEntry = new FakeEntryImpl(
      'Recent', VolumeManagerCommon.RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.RecentFileType.ALL);
  fileTypeFiltersController = new FileTypeFiltersController(
      container, directoryModel, recentEntry, mockA11y);

  // Create a directory entry which is not Recents to simulate directory change.
  myFilesEntry =
      new EntryList('My Files', VolumeManagerCommon.RootType.MY_FILES);
}

/**
 * Tests that creating FileTypeFiltersController generates four buttons in the
 * given container element.
 */
export function testCreatedButtonLabels() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  assertEquals(buttons[0].textContent, 'All');
  assertEquals(buttons[1].textContent, 'Audio');
  assertEquals(buttons[2].textContent, 'Documents');
  assertEquals(buttons[3].textContent, 'Images');
  assertEquals(buttons[4].textContent, 'Videos');
}

/**
 * Tests that initial states of all buttons inside container are inactive
 * except the first button (button with label "All").
 */
export function testButtonInitialActiveState() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));
}

/**
 * Tests that click events can toggle button state (active <-> inactive),
 * if the button is already active, make it inactive and make "All" button
 * active.
 */
export function testButtonToggleState() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  // State change: inactive -> active -> inactive.
  assertFalse(buttons[1].classList.contains('active'));
  buttons[1].click();
  assertTrue(buttons[1].classList.contains('active'));
  buttons[1].click();
  assertFalse(buttons[1].classList.contains('active'));
  assertTrue(buttons[0].classList.contains('active'));
  // Clicking active "All" does nothing.
  buttons[0].click();
  assertTrue(buttons[0].classList.contains('active'));
}

/**
 * Tests that only one button can be active.
 * If button_1 is clicked when button_0 is active, button_0 becomes inactive and
 * button_1 becomes active.
 */
export function testOnlyOneButtonCanActive() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  assertTrue(buttons[0].classList.contains('active'));

  assertFalse(buttons[1].classList.contains('active'));
  buttons[1].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertTrue(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  buttons[2].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertTrue(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  buttons[3].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertTrue(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  buttons[0].click();
  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));
}

/**
 * Tests that container element is visible only when the current directory is
 * Recents view.
 */
export function testContainerIsShownOnlyInRecents() {
  container.hidden = true;
  directoryModel.changeDirectoryEntry(recentEntry);
  assertFalse(container.hidden);
  directoryModel.changeDirectoryEntry(myFilesEntry);
  assertTrue(container.hidden);
}

/**
 * Tests that button's active state is reset when the user leaves
 * Recents view and go back again.
 */
export function testActiveButtonIsResetOnLeavingRecents() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  directoryModel.changeDirectoryEntry(recentEntry);

  buttons[1].click();
  assertFalse(buttons[0].classList.contains('active'));
  assertTrue(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  // Changing directory to the same Recent doesn't reset states.
  directoryModel.changeDirectoryEntry(recentEntry);
  assertFalse(buttons[0].classList.contains('active'));
  assertTrue(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  directoryModel.changeDirectoryEntry(myFilesEntry);
  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));

  directoryModel.changeDirectoryEntry(recentEntry);
  assertTrue(buttons[0].classList.contains('active'));
  assertFalse(buttons[1].classList.contains('active'));
  assertFalse(buttons[2].classList.contains('active'));
  assertFalse(buttons[3].classList.contains('active'));
  assertFalse(buttons[4].classList.contains('active'));
}

/**
 * Tests that the active state of each button is reflected to the Recent entry's
 * recentFileType property, and DirectoryModel.rescan() is called after the
 * Recent entry's property is modified.
 */
export function testAppliedFilters() {
  const buttons = container.children;
  assertEquals(buttons.length, TOTAL_FILTER_BUTTON_COUNT);

  directoryModel.changeDirectoryEntry(recentEntry);

  buttons[1].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.AUDIO);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;

  // Clicking an active button will trigger a scan for "All".
  buttons[1].click();
  assertEquals(
      recentEntry.recentFileType, chrome.fileManagerPrivate.RecentFileType.ALL);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;

  buttons[2].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.DOCUMENT);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;

  buttons[3].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.IMAGE);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;

  buttons[4].click();
  assertEquals(
      recentEntry.recentFileType,
      chrome.fileManagerPrivate.RecentFileType.VIDEO);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;

  buttons[0].click();
  assertEquals(
      recentEntry.recentFileType, chrome.fileManagerPrivate.RecentFileType.ALL);
  assertTrue(window.isScanCalled);
  window.isScanCalled = false;
}
