// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {getDriveQuotaMetadata, getSizeStats} from '../../common/js/api.js';
import {isFakeEntry} from '../../common/js/entry_utils.js';
import {str} from '../../common/js/translations.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

import type {DirectoryModel} from './directory_model.js';
import {type DirectoryChangeEvent} from './directory_model.js';
import type {ProvidersModel} from './providers_model.js';
import type {GearMenu} from './ui/gear_menu.js';
import {type SpaceInfo} from './ui/gear_menu.js';
import type {MultiMenuButton} from './ui/multi_menu_button.js';
import type {ProvidersMenu} from './ui/providers_menu.js';

export class GearMenuController {
  constructor(
      private readonly gearButton_: MultiMenuButton,
      private readonly gearMenu_: GearMenu,
      private readonly providersMenu_: ProvidersMenu,
      private readonly directoryModel_: DirectoryModel,
      private readonly providersModel_: ProvidersModel) {
    this.gearButton_.addEventListener(
        'menushow', this.onShowGearMenu_.bind(this));
    this.directoryModel_.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  private onShowGearMenu_() {
    this.refreshRemainingSpace_();

    this.providersModel_.getMountableProviders().then(providers => {
      const shouldHide = providers.length === 0;
      if (!shouldHide) {
        // Trigger an update of the providers submenu.
        this.providersMenu_.updateSubMenu();
      }
      this.gearMenu_.updateShowProviders(shouldHide);
    });
  }

  private onDirectoryChanged_(directoryChangeEvent: DirectoryChangeEvent) {
    if (directoryChangeEvent.detail.volumeChanged) {
      this.refreshRemainingSpace_();
    }

    if (this.gearButton_.isMenuShown()) {
      assert(this.gearButton_.menu);
      this.gearButton_.menu.updateCommands(this.gearButton_);
    }
  }

  /**
   * Refreshes space info of the current volume.
   */
  private refreshRemainingSpace_() {
    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirectory || isFakeEntry(currentDirectory)) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // TODO(mtomasz): Add support for remaining space indication for provided
    // file systems.
    if (currentVolumeInfo.volumeType === VolumeType.PROVIDED ||
        currentVolumeInfo.volumeType === VolumeType.MEDIA_VIEW ||
        currentVolumeInfo.volumeType === VolumeType.ARCHIVE) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    if (currentVolumeInfo.volumeType === VolumeType.DRIVE) {
      this.gearMenu_.setSpaceInfo(
          getDriveQuotaMetadata(currentDirectory)
              .then(
                  (quota: chrome.fileManagerPrivate.DriveQuotaMetadata|
                   undefined) => {
                    if (!quota) {
                      return;
                    }
                    return {
                      totalSize: quota.totalBytes,
                      usedSize: quota.usedBytes,
                      warningMessage: quota.organizationLimitExceeded ?
                          str('DRIVE_ORGANIZATION_STORAGE_FULL') :
                          null,
                    } as SpaceInfo;
                  }),
          true);
      return;
    }

    this.gearMenu_.setSpaceInfo(
        getSizeStats(currentVolumeInfo.volumeId)
            .then(
                (size: chrome.fileManagerPrivate.MountPointSizeStats|
                 undefined) => {
                  if (!size) {
                    return;
                  }
                  return {
                    totalSize: size.totalSize,
                    usedSize: size.totalSize - size.remainingSize,
                  } as SpaceInfo;
                }),
        true);
  }

  /**
   * Handles preferences change and updates menu.
   */
  private onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      if (chrome.runtime.lastError) {
        return;
      }

      if (prefs.driveSyncEnabledOnMeteredNetwork) {
        this.gearMenu_.syncButton.setAttribute('checked', '');
      } else {
        this.gearMenu_.syncButton.removeAttribute('checked');
      }
    });
  }
}
