// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {FileFilter} from './directory_contents.js';
import type {DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import {NamingController} from './naming_controller.js';
import type {ConfirmDialog} from './ui/dialogs.js';
import type {FilesAlertDialog} from './ui/files_alert_dialog.js';
import type {ListContainer} from './ui/list_container.js';

interface Harness {
  controller: NamingController;
  renameInput: HTMLInputElement;
  setEntry: (name: string, isFile: boolean) => void;
}

function createHarness(): Harness {
  document.body.replaceChildren();
  const host = document.createElement('div');
  const item = Object.assign(document.createElement('li'), {listIndex: 0});
  const labelContainer = document.createElement('div');
  const label = document.createElement('span');
  label.className = 'filename-label';
  labelContainer.appendChild(label);
  item.appendChild(labelContainer);
  host.appendChild(item);
  document.body.appendChild(host);

  const renameInput = document.createElement('input');
  renameInput.className = 'rename entry-name';

  let currentEntry = {name: '', isFile: false, isDirectory: true} as Entry;

  const listContainer = {
    renameInput,
    selectionModel: {selectedIndex: 0},
    currentList: {
      getListItemByIndex: (index: number) => (index === 0 ? item : null),
      dataModel: {
        item: () => currentEntry,
      },
      focus: () => {},
    },
    findListItemForNode: () => item,
    startBatchUpdates: () => {},
    endBatchUpdates: () => {},
  } as unknown as ListContainer;

  const controller = new NamingController(
      listContainer,
      {} as FilesAlertDialog,
      {} as ConfirmDialog,
      {} as DirectoryModel,
      {} as FileFilter,
      {} as FileSelectionHandler,
  );

  return {
    controller,
    renameInput,
    setEntry: (name: string, isFile: boolean) => {
      label.textContent = name;
      currentEntry = {name, isFile, isDirectory: !isFile} as Entry;
    },
  };
}

function assertSelectionBeforeFocus(
    harness: Harness, name: string, isFile: boolean, expectedEnd: number) {
  const {controller, renameInput, setEntry} = harness;
  setEntry(name, isFile);

  let selectionAtFocus: [number|null, number|null] = [null, null];
  renameInput.addEventListener('focus', () => {
    selectionAtFocus = [renameInput.selectionStart, renameInput.selectionEnd];
  }, {once: true});

  controller.initiateRename();

  assertEquals(renameInput, document.activeElement);
  assertTrue(controller.isRenamingInProgress());
  assertEquals(0, selectionAtFocus[0]);
  assertEquals(expectedEnd, selectionAtFocus[1]);
  assertEquals(0, renameInput.selectionStart);
  assertEquals(expectedEnd, renameInput.selectionEnd);
}

export function testInitiateRenameSelectsFolderNameBeforeFocus() {
  const harness = createHarness();
  assertSelectionBeforeFocus(harness, 'New folder', /*isFile=*/ false, 10);

  // Cancel the active rename via blur and verify reusing the same renameInput
  // for a folder whose name contains a dot selects the entire folder name.
  harness.renameInput.blur();
  assertFalse(harness.controller.isRenamingInProgress());
  assertSelectionBeforeFocus(harness, 'folder.v1', /*isFile=*/ false, 9);
}

export function testInitiateRenameSelectsFileStemBeforeFocus() {
  const harness = createHarness();
  assertSelectionBeforeFocus(harness, 'archive.tar.gz', /*isFile=*/ true, 11);
}

export function testInitiateRenameSelectsExtensionlessFileBeforeFocus() {
  const harness = createHarness();
  assertSelectionBeforeFocus(harness, 'README', /*isFile=*/ true, 6);
}
