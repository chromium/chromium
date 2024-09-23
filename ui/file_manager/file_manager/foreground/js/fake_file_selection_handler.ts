// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {AllowedPaths} from '../../common/js/volume_manager_types.js';
import {updateDirectoryContent, updateSelection} from '../../state/ducks/current_directory.js';
import {PropStatus} from '../../state/state.js';
import type {Store} from '../../state/store.js';

import {type FileSelection, FileSelectionHandler} from './file_selection.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import type {ListContainer} from './ui/list_container.js';

/**
 * Mock FileSelectionHandler.
 */
export class FakeFileSelectionHandler extends FileSelectionHandler {
  private eventTarget_ = new EventTarget();

  constructor() {
    super(
        createFakeDirectoryModel(),
        document.createElement('div') as unknown as ListContainer,
        new MockMetadataModel({}) as unknown as MetadataModel,
        new MockVolumeManager(),
        AllowedPaths.ANY_PATH,
    );
    this.selection = {} as FileSelection;
    this.updateSelection([], []);
  }

  computeAdditionalCallback() {}

  updateSelection(
      entries: Array<Entry|FilesAppEntry>, mimeTypes: string[], store?: Store) {
    this.selection = {
      entries: entries,
      mimeTypes: mimeTypes,
      computeAdditional: async (_metadataModel) => {
        this.computeAdditionalCallback();
        return Promise.resolve(true);
      },
    } as FileSelection;

    if (store) {
      // Make sure that the entry is in the directory content.
      store.dispatch(
          updateDirectoryContent({entries, status: PropStatus.SUCCESS}));
      // Mark the entry as selected.
      store.dispatch(updateSelection({
        selectedKeys: entries.map(e => e.toURL()),
        entries,
      }));
    }
  }

  override addEventListener(...args: any[]) {
    return this.eventTarget_.addEventListener(
        ...args as Parameters<EventTarget['addEventListener']>);
  }

  override isAvailable() {
    return true;
  }
}
