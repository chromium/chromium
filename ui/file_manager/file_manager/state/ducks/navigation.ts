// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isOneDrive, isOneDriveId} from '../../common/js/entry_utils.js';
import type {EntryList, FilesAppEntry, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isSkyvaultV2Enabled} from '../../common/js/flags.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';
import {Slice} from '../../lib/base_store.js';
import {type AndroidApp, DialogType, type NavigationKey, type NavigationRoot, NavigationSection, NavigationType, type State, type Volume} from '../../state/state.js';
import {getMyFiles} from '../ducks/all_entries.js';
import {driveRootEntryListKey, oneDriveFakeRootKey, recentRootKey, trashRootKey} from '../ducks/volumes.js';
import {getEntry} from '../store.js';

/**
 * @fileoverview Navigation slice of the store.
 */

const slice = new Slice<State, State['navigation']>('navigation');
export {slice as navigationSlice};

const sections = new Map<VolumeType, NavigationSection>();
// My Files.
sections.set(VolumeType.DOWNLOADS, NavigationSection.MY_FILES);
// Cloud.
sections.set(VolumeType.DRIVE, NavigationSection.CLOUD);
sections.set(VolumeType.SMB, NavigationSection.CLOUD);
sections.set(VolumeType.PROVIDED, NavigationSection.CLOUD);
sections.set(VolumeType.DOCUMENTS_PROVIDER, NavigationSection.CLOUD);
// Removable.
sections.set(VolumeType.REMOVABLE, NavigationSection.REMOVABLE);
sections.set(VolumeType.MTP, NavigationSection.REMOVABLE);
sections.set(VolumeType.ARCHIVE, NavigationSection.REMOVABLE);

/** Returns the entry for the volume's top-most prefix or the volume itself. */
function getPrefixEntryOrEntry(state: State, volume: Volume): VolumeEntry|
    EntryList|null {
  if (volume.prefixKey) {
    const entry = getEntry(state, volume.prefixKey);
    return entry as VolumeEntry | EntryList | null;
  }
  if (volume.volumeType === VolumeType.DOWNLOADS) {
    return getMyFiles(state)!.myFilesEntry;
  }

  const entry = getEntry(state, volume.rootKey!);
  return entry as VolumeEntry | EntryList | null;
}

/**
 * Create action to refresh all navigation roots. This will clear all existing
 * navigation roots in the store and regenerate them with the current state
 * data.
 *
 * Navigation roots' Entries/Volumes will be ordered as below:
 *  1. Recents.
 *  2. Shortcuts.
 *  3. "My-Files" (grouping), actually Downloads volume.
 *  4. Google Drive.
 *  5. ODFS.
 *  6. SMBs
 *  7. Other FSP (File System Provider) (when mounted).
 *  8. Other volumes (MTP, ARCHIVE, REMOVABLE).
 *  9. Android apps.
 *  10. Trash.
 */
export const refreshNavigationRoots =
    slice.addReducer('refresh-roots', refreshNavigationRootsReducer);

function refreshNavigationRootsReducer(currentState: State): State {
  const {
    navigation: {roots: previousRoots},
    folderShortcuts,
    androidApps,
    materializedViews,
  } = currentState;

  /** Roots in the desired order. */
  const roots: NavigationRoot[] = [];
  /** Set to avoid adding the same entry multiple times. */
  const processedEntryKeys = new Set<NavigationKey>();

  // Add the Recent/Materialized view root.
  const recentRoot = previousRoots.find(root => root.key === recentRootKey);
  if (recentRoot) {
    roots.push(recentRoot);
    processedEntryKeys.add(recentRootKey);
  } else {
    const recentEntry =
        getEntry(currentState, recentRootKey) as FilesAppEntry | null;
    if (recentEntry) {
      roots.push({
        key: recentRootKey,
        section: NavigationSection.TOP,
        separator: false,
        type: NavigationType.RECENT,
      });
      processedEntryKeys.add(recentRootKey);
    }
  }

  // Add Starred files.
  for (const view of materializedViews) {
    if (view.isRoot) {
      roots.push({
        key: view.key,
        section: NavigationSection.TOP,
        separator: false,
        type: NavigationType.MATERIALIZED_VIEW,
      });
    }
  }

  // Add the Shortcuts.
  // TODO: Since Shortcuts are only for Drive, do we need to remove shortcuts
  // if Drive isn't available anymore?
  folderShortcuts.forEach(shortcutKey => {
    const shortcutEntry =
        getEntry(currentState, shortcutKey) as FilesAppEntry | null;
    if (shortcutEntry) {
      roots.push({
        key: shortcutKey,
        section: NavigationSection.TOP,
        separator: false,
        type: NavigationType.SHORTCUT,
      });
      processedEntryKeys.add(shortcutKey);
    }
  });

  // MyFiles.
  const {myFilesEntry, myFilesVolume} = getMyFiles(currentState);
  if (myFilesEntry) {
    roots.push({
      key: myFilesEntry.toURL(),
      section: NavigationSection.MY_FILES,
      // Only show separator if this is not the first navigation item.
      separator: processedEntryKeys.size > 0,
      type: myFilesVolume ? NavigationType.VOLUME : NavigationType.ENTRY_LIST,
    });
    processedEntryKeys.add(myFilesEntry.toURL());
  }

  // Add Google Drive - the only Drive.
  // When drive pref changes from enabled to disabled, we remove the drive root
  // key from the `state.uiEntries` immediately, but the drive root entry itself
  // is removed asynchronously, so here we need to check both, if the key
  // doesn't exist any more, we shouldn't render Drive item even if the drive
  // root entry is still available.
  const driveEntryKeyExist =
      currentState.uiEntries.includes(driveRootEntryListKey);
  const driveEntry =
      getEntry(currentState, driveRootEntryListKey) as EntryList | null;
  if (driveEntryKeyExist && driveEntry) {
    roots.push({
      key: driveEntry.toURL(),
      section: NavigationSection.GOOGLE_DRIVE,
      separator: true,
      type: NavigationType.DRIVE,
    });
    processedEntryKeys.add(driveEntry.toURL());
  }

  // Add OneDrive placeholder if needed.
  // OneDrive is always added directly below Drive.
  if (isSkyvaultV2Enabled()) {
    const oneDriveUIEntryExists =
        currentState.uiEntries.includes(oneDriveFakeRootKey);
    const oneDriveVolumeExists =
        Object.values<Volume>(currentState.volumes).find(v => isOneDrive(v)) !==
        undefined;
    if (oneDriveUIEntryExists && !oneDriveVolumeExists) {
      roots.push({
        key: oneDriveFakeRootKey,
        section: NavigationSection.ODFS,
        separator: true,
        type: NavigationType.VOLUME,
      });
      processedEntryKeys.add(oneDriveFakeRootKey);
    }
  }

  // Other volumes.
  const volumesOrder: Partial<Record<VolumeType, number>> = {
    // ODFS is a PROVIDED volume type but is a special case to be directly
    // below Drive.
    // ODFS : 0
    [VolumeType.SMB]: 1,
    [VolumeType.PROVIDED]: 2,  // FSP.
    [VolumeType.DOCUMENTS_PROVIDER]: 3,
    [VolumeType.REMOVABLE]: 4,
    [VolumeType.ARCHIVE]: 5,
    [VolumeType.MTP]: 6,
  };
  // Filter volumes based on the volumeInfoList in volumeManager.
  const {volumeManager} = window.fileManager;
  const filteredVolumes =
      Object.values<Volume>(currentState.volumes).filter(volume => {
        const volumeEntry =
            getEntry(currentState, volume.rootKey!) as VolumeEntry;
        return volumeManager.isAllowedVolume(volumeEntry.volumeInfo);
      });

  function getVolumeOrder(volume: Volume) {
    if (isOneDriveId(volume.providerId)) {
      return 0;
    }
    return volumesOrder[volume.volumeType] ?? 999;
  }

  const volumes = filteredVolumes
                      .filter((v) => {
                        return (
                            // Only display if the entry is resolved.
                            v.rootKey &&
                            // MyFiles and Drive is already displayed above.
                            // MediaView volumeType isn't displayed.
                            !(v.volumeType === VolumeType.DOWNLOADS ||
                              v.volumeType === VolumeType.DRIVE ||
                              v.volumeType === VolumeType.MEDIA_VIEW));
                      })
                      .sort((v1, v2) => {
                        const v1Order = getVolumeOrder(v1);
                        const v2Order = getVolumeOrder(v2);
                        return v1Order - v2Order;
                      });
  let lastSection: NavigationSection|null = null;
  for (const volume of volumes) {
    // Some volumes might be nested inside another volume or entry list, e.g.
    // Multiple partition removable volumes can be nested inside a EntryList, or
    // GuestOS/Crostini/Android volumes will be nested inside MyFiles, for these
    // volumes, we only need to add its parent volume in the navigation roots.
    const volumeEntry = getPrefixEntryOrEntry(currentState, volume);

    if (volumeEntry && !processedEntryKeys.has(volumeEntry.toURL())) {
      let section =
          sections.get(volume.volumeType) ?? NavigationSection.REMOVABLE;
      if (isOneDriveId(volume.providerId)) {
        section = NavigationSection.ODFS;
      }
      const isSectionStart = section !== lastSection;
      roots.push({
        key: volumeEntry.toURL(),
        section,
        separator: isSectionStart,
        type: NavigationType.VOLUME,
      });
      processedEntryKeys.add(volumeEntry.toURL());
      lastSection = section;
    }
  }

  // Android Apps.
  Object.values(androidApps as Record<string, AndroidApp>)
      .forEach((app, index) => {
        roots.push({
          key: app.packageName,
          section: NavigationSection.ANDROID_APPS,
          separator: index === 0,
          type: NavigationType.ANDROID_APPS,
        });
        processedEntryKeys.add(app.packageName);
      });

  // Trash.
  // Trash should only show when Files app is open as a standalone app. The ARC
  // file selector, however, opens Files app as a standalone app but passes a
  // query parameter to indicate the mode. As Trash is a fake volume, it is
  // not filtered out in the filtered volume manager so perform it here
  // instead.
  const {dialogType} = window.fileManager;
  const shouldShowTrash = dialogType === DialogType.FULL_PAGE &&
      !volumeManager.getMediaStoreFilesOnlyFilterEnabled();
  // When trash pref changes from enabled to disabled, we remove the trash root
  // key from the `state.uiEntries` immediately, but the trash entry itself is
  // removed asynchronously, so here we need to check both, if the key doesn't
  // exist any more, we shouldn't render Trash item even if the trash entry is
  // still available.
  const trashEntryKeyExist = currentState.uiEntries.includes(trashRootKey);
  const trashEntry =
      getEntry(currentState, trashRootKey) as FilesAppEntry | null;
  if (shouldShowTrash && trashEntryKeyExist && trashEntry) {
    roots.push({
      key: trashRootKey,
      section: NavigationSection.TRASH,
      separator: true,
      type: NavigationType.TRASH,
    });
    processedEntryKeys.add(trashRootKey);
  }

  return {
    ...currentState,
    navigation: {
      roots,
    },
  };
}
