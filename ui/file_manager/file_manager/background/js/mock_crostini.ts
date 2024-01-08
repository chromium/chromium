// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

import {Crostini} from './crostini.js';
import type {VolumeManager} from './volume_manager.js';

/**
 * Crostini shared path state handler.
 */
class MockCrostini extends Crostini {
  override initEnabled() {}

  override initVolumeManager(_volumeManager: VolumeManager) {}

  override setEnabled(
      _vmName: string, _containerName: string, _enabled: boolean) {}

  override isEnabled(_vmName: string): boolean {
    return false;
  }

  override registerSharedPath(_vmName: string, _entry: Entry) {}

  override unregisterSharedPath(_vmName: string, _entry: Entry) {}

  override isPathShared(_vmName: string, _entry: Entry): boolean {
    return false;
  }

  override canSharePath(
      _vmName: string, _entry: Entry|FilesAppEntry,
      _persist: boolean): boolean {
    return false;
  }
}


/**
 * Crostini shared path state handler factory for foreground tests. Change it
 * to a mock when tests need to override {CrostiniImpl} behavior.
 */
export function createCrostiniForTest(): Crostini {
  return new MockCrostini();
}
