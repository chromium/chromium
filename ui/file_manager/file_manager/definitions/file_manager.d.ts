// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FileManagerBase} from '../background/js/file_manager_base.js';
import type {VolumeManager} from '../background/js/volume_manager.js';
import type {FilesAppEntry} from '../common/js/files_app_entry_types.js';
import type {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import type {FileManagerUI} from '../foreground/js/ui/file_manager_ui.js';


interface FileFilter {
  filter(entry: Entry|FilesAppEntry): boolean;
}
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
  fileFilter: FileFilter;
  directoryModel: DirectoryModel;
  directoryTreeNamingController: DirectoryTreeNamingController;
  ui: FileManagerUI;
  getLastVisitedUrl(): string;
  getTranslatedString(id: string): string;
  onUnloadForTest(): void;
}

/**
 * The singleton instance for FileManager is available in the Window object.
 */
declare global {
  interface Window {
    appID: string;
    fileManager: FileManager;
    IN_TEST: boolean;
    JSErrorCount: number;
    store: Store;

    /** Namespace used for test utils. */
    test: any;

    // Only used for grid.ts
    cvox?: {
      Api?: {
        isChromeVoxActive: () => boolean,
      },
    };

    // Defined in the file_manager_base.ts
    background: FileManagerBase;

    // Defined in the main_window_component.ts
    isFocused?: () => boolean;

    // For unit test.
    chrome: typeof chrome;
  }

  function webkitResolveLocalFileSystemURL(
      url: string, successCallback: FileSystemEntryCallback,
      errorCallback: ErrorCallback): void;
}

export {};
