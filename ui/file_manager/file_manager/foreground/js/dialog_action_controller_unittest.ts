// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {DialogType} from '../../state/state.js';

import {DialogActionController} from './dialog_action_controller.js';
import {FileFilter} from './directory_contents.js';
import {FakeFileSelectionHandler} from './fake_file_selection_handler.js';
import {LaunchParam} from './launch_param.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import type {NamingController} from './naming_controller.js';
import {DialogFooter} from './ui/dialog_footer.js';

// Mock the same DOM structure as required by DialogFooter constructor.
function constructFooterElement(): HTMLElement {
  const footerElement = document.createElement('div');

  const selectElement = document.createElement('div');
  selectElement.className = 'file-type';
  const options = document.createElement('div');
  options.className = 'options';
  const label = document.createElement('span');
  selectElement.appendChild(label);
  selectElement.appendChild(options);
  footerElement.appendChild(selectElement);

  const okButton = document.createElement('button');
  okButton.className = 'ok';
  const buttonLabel = document.createElement('span');
  okButton.appendChild(buttonLabel);
  footerElement.appendChild(okButton);

  const cancelButton = document.createElement('button');
  cancelButton.className = 'cancel';
  footerElement.appendChild(cancelButton);

  const newFolderButton = document.createElement('button');
  newFolderButton.id = 'new-folder-button';
  footerElement.appendChild(newFolderButton);

  document.body.appendChild(footerElement);
  const newFolderCommand = document.createElement('command');
  newFolderCommand.id = 'new-folder';
  document.body.appendChild(newFolderCommand);

  return footerElement;
}

export function testFilterFilesWithSpecialCharactersExtension() {
  const dialogType = DialogType.SELECT_SAVEAS_FILE;
  const volumeManager = new MockVolumeManager();
  const fileSelectionHandler = new FakeFileSelectionHandler();
  const fileFilter = new FileFilter(volumeManager);
  const footerElement = constructFooterElement();

  new DialogActionController(
      dialogType,
      new DialogFooter(
          dialogType, footerElement, document.createElement('cr-input')),
      createFakeDirectoryModel(),
      volumeManager,
      fileFilter,
      {} as NamingController,
      fileSelectionHandler,
      new LaunchParam({
        typeList: [
          {extensions: ['*'], description: 'any', selected: false},
          {extensions: ['c++', 'h++'], description: 'custom', selected: true},
        ],
      }),
  );

  // The constructor will call onFileTypeFilterChanged_(), which will call
  // regexpForCurrentFilter_() and add the regex to the `fileFilter`, that's
  // where we can test the regular expression.
  // Remove all other filters to make sure we are testing fileType filter.
  fileFilter.removeFilter('hidden');
  fileFilter.removeFilter('android_hidden');
  fileFilter.removeFilter('android_download');

  const entry1 = {name: 'a.c++', isDirectory: false} as any as Entry;
  const entry2 = {name: 'b.h++', isDirectory: false} as any as Entry;
  const entry3 = {name: 'c.html', isDirectory: false} as any as Entry;
  assertTrue(fileFilter.filter(entry1));
  assertTrue(fileFilter.filter(entry2));
  assertFalse(fileFilter.filter(entry3));
}
