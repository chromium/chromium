// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from '../externs/volume_manager.js';

/**
 * Type definition for foreground/js/file_manager.js:FileManager.
 *
 * For now only defining the bare minimum.
 */
interface FileManager {
  volumeManager: VolumeManager;
}

/**
 * The singleton instance for FileManager is available in the Window object.
 */
declare global {
  interface Window {
    fileManager: FileManager;
  }
}

export {};
