// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from '../externs/volume_manager.js';
import {MetadataModel} from '../foreground/js/metadata/metadata_model.js';

/**
 * Type definition for foreground/js/file_manager.js:FileManager.
 *
 * For now only defining the bare minimum.
 */
interface FileManager {
  volumeManager: VolumeManager;
  metadataModel: MetadataModel;
  crostini: Crostini;
  selectionHandler: FileSelectionHandler;
  taskController: TaskController;
  dialogType: DialogType;
  directoryModel: DirectoryModel;
  directoryTreeNamingController: DirectoryTreeNamingController;
}

/**
 * The singleton instance for FileManager is available in the Window object.
 */
declare global {
  interface Window {
    fileManager: FileManager;
    IN_TEST: boolean;
    store: Store;
    /** Log action data in the console for debugging purpose. */
    DEBUG_STORE: boolean;
  }
}

export {};
