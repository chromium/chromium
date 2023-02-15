// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDriveQuotaMetadata, getSizeStats} from '../../common/js/api.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';

import {DirectoryModel} from './directory_model.js';
import {CommandHandler} from './file_manager_commands.js';
import {ProvidersModel} from './providers_model.js';
import {GearMenu} from './ui/gear_menu.js';
import {MultiMenuButton} from './ui/multi_menu_button.js';
import {ProvidersMenu} from './ui/providers_menu.js';

export class GearMenuController {
  /**
   * @param {!MultiMenuButton} gearButton
   * @param {!GearMenu} gearMenu
   * @param {!ProvidersMenu} providersMenu
   * @param {!DirectoryModel} directoryModel
   * @param {!CommandHandler} commandHandler
   * @param {!ProvidersModel} providersModel
   */
  constructor(
      gearButton, gearMenu, providersMenu, directoryModel, commandHandler,
      providersModel) {
    /** @private @const {!MultiMenuButton} */
    this.gearButton_ = gearButton;

    /** @private @const {!GearMenu} */
    this.gearMenu_ = gearMenu;

    /** @private @const {!ProvidersMenu} */
    this.providersMenu_ = providersMenu;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!CommandHandler} */
    this.commandHandler_ = commandHandler;

    /** @private @const {!ProvidersModel} */
    this.providersModel_ = providersModel;

    gearButton.addEventListener('menushow', this.onShowGearMenu_.bind(this));
    directoryModel.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  /**
   * @private
   */
  onShowGearMenu_() {
    this.refreshRemainingSpace_(false); /* Without loading caption. */

    this.providersModel_.getMountableProviders().then(providers => {
      const shouldHide = providers.length == 0;
      if (!shouldHide) {
        // Trigger an update of the providers submenu.
        this.providersMenu_.updateSubMenu();
      }
      this.gearMenu_.updateShowProviders(shouldHide);
    });
  }

  /**
   * @param {Event} event
   * @private
   */
  onDirectoryChanged_(event) {
    event = /** @type {DirectoryChangeEvent} */ (event);
    if (event.volumeChanged) {
      this.refreshRemainingSpace_(true);
    }  // Show loading caption.

    if (this.gearButton_.isMenuShown()) {
      this.gearButton_.menu.updateCommands(this.gearButton_);
    }
  }

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   * @private
   */
  refreshRemainingSpace_(showLoadingCaption) {
    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirectory || util.isRecentRoot(currentDirectory)) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    const currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // TODO(mtomasz): Add support for remaining space indication for provided
    // file systems.
    if (currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.PROVIDED ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.MEDIA_VIEW ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.ARCHIVE) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    if (currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.DRIVE) {
      this.gearMenu_.setSpaceInfo(
          getDriveQuotaMetadata(currentDirectory)
              .then(
                  quota /* chrome.fileManagerPrivate.DriveQuotaMetadata */ => ({
                    totalSize: quota.totalBytes,
                    usedSize: quota.usedBytes,
                    warningMessage: quota.organizationLimitExceeded ?
                        strf('DRIVE_ORGANIZATION_STORAGE_FULL') :
                        null,
                  })),
          true);
      return;
    }

    this.gearMenu_.setSpaceInfo(
        getSizeStats(currentVolumeInfo.volumeId)
            .then(size /* chrome.fileManagerPrivate.MountPointSizeStats */ => ({
                    totalSize: size.totalSize,
                    usedSize: size.totalSize - size.remainingSize,
                  })),
        true);
  }

  /**
   * Handles preferences change and updates menu.
   * @private
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      if (chrome.runtime.lastError) {
        return;
      }

      if (prefs.cellularDisabled) {
        this.gearMenu_.syncButton.setAttribute('checked', '');
      } else {
        this.gearMenu_.syncButton.removeAttribute('checked');
      }
    });
  }
}
