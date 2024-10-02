// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FileKey} from '../../state/state.js';
import {DialogType, PropStatus, type State} from '../../state/state.js';
import {getFileData} from '../../state/store.js';

import {recordEnum} from './metrics.js';
import {VolumeType} from './shared_types.js';
import {debug} from './util.js';
import {RootType} from './volume_manager_types.js';

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
 * Keep the name and the value in sync with `FileManagerNavigationSurface` in
 * //tools/metrics/histograms/metadata/file/enums.xml
 */
export enum NavigationSurface {
  PHYSCIAL_LOCATION = '0',
  SEARCH_RESULTS = '1',
  RECENT = '2',
  STARRED_FILES = '3',
  SCREEN_CAPTURES_VIEW = '4',
  DRIVE_SHARED_WITH_ME = '5',
  DRIVE_OFFLINE = '6',
}

// The ordering is important.
export const UMA_NAVIGATION_SURFACES = [
  NavigationSurface.PHYSCIAL_LOCATION,
  NavigationSurface.SEARCH_RESULTS,
  NavigationSurface.RECENT,
  NavigationSurface.STARRED_FILES,
  NavigationSurface.SCREEN_CAPTURES_VIEW,
  NavigationSurface.DRIVE_SHARED_WITH_ME,
  NavigationSurface.DRIVE_OFFLINE,
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
    debug(`Unknown volume type: ${volumeType} for key ${fileKey}`);
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

/**
 * Records the action of opening a file by the file navigation surface.*
 */
export function recordViewingNavigationSurfaceUma(state: State): void {
  const currentDirectoryKey = state.currentDirectory?.key;
  const rootType = state.currentDirectory?.rootType;
  const search =
      !!state.search?.query && state.search.status === PropStatus.SUCCESS;

  if (!currentDirectoryKey && !search) {
    return;
  }

  let surface = NavigationSurface.PHYSCIAL_LOCATION;
  if (search) {
    surface = NavigationSurface.SEARCH_RESULTS;
  } else if (rootType === RootType.RECENT) {
    surface = NavigationSurface.RECENT;
  } else if (rootType === RootType.DRIVE_SHARED_WITH_ME) {
    surface = NavigationSurface.DRIVE_SHARED_WITH_ME;
  } else if (rootType === RootType.DRIVE_OFFLINE) {
    surface = NavigationSurface.DRIVE_OFFLINE;
  }

  recordEnum(
      appendAppMode(`ViewingNavigationSurface`, state), surface,
      UMA_NAVIGATION_SURFACES);
}
