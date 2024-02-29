// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FileKey} from '../../state/state.js';
import {DialogType, type State} from '../../state/state.js';
import {getFileData} from '../../state/store.js';

import {recordEnum} from './metrics.js';
import {VolumeType} from './shared_types.js';

/**
 * Keep the order of this in sync with FileManagerVolumeType in
 * tools/metrics/histograms/enums.xml.
 */
export const UMA_VOLUME_TYPES = [
  VolumeType.DRIVE,
  VolumeType.DOWNLOADS,
  VolumeType.REMOVABLE,
  VolumeType.ARCHIVE,
  VolumeType.PROVIDED,
  VolumeType.MTP,
  VolumeType.MEDIA_VIEW,
  VolumeType.CROSTINI,
  VolumeType.ANDROID_FILES,
  VolumeType.DOCUMENTS_PROVIDER,
  VolumeType.SMB,
  VolumeType.SYSTEM_INTERNAL,
  VolumeType.GUEST_OS,
];

/**
 * Records the action of opening a file by the file volume type.
 */
export function recordViewingVolumeTypeUma(
    state: State, fileKey: FileKey): void {
  const fileData = getFileData(state, fileKey);
  if (!fileData || !fileData.volumeId) {
    return;
  }

  const volumeType = state.volumes[fileData.volumeId!]?.volumeType;
  if (!volumeType) {
    return;
  }

  if (!UMA_VOLUME_TYPES.includes(volumeType)) {
    console.debug(`Unknown volume type: ${volumeType} for key ${fileKey}`);
    console.warn(`Unknown volume type: ${volumeType}`);
    return;
  }

  recordEnum(
      appendAppMode(`ViewingVolumeType`, state), volumeType, UMA_VOLUME_TYPES);
}

function appendAppMode(name: string, state: State) {
  const dialogType = state.launchParams.dialogType;
  const appMode =
      (dialogType === DialogType.SELECT_SAVEAS_FILE || !dialogType) ? 'Other' :
      dialogType === DialogType.FULL_PAGE ? 'Standalone' :
                                            'FilePicker';
  return `${name}.${appMode}`;
}
