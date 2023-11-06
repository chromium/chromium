// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDriveQuotaMetadata, getSizeStats} from '../../common/js/api.js';
import {isRecentRoot} from '../../common/js/entry_utils.js';
import {str} from '../../common/js/translations.js';
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
    /** @private @const @type {!MultiMenuButton} */
    this.gearButton_ = gearButton;

    /** @private @const @type {!GearMenu} */
    this.gearMenu_ = gearMenu;

    /** @private @const @type {!ProvidersMenu} */
    this.providersMenu_ = providersMenu;

    /** @private @const @type {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const @type {!CommandHandler} */
    this.commandHandler_ = commandHandler;

    /** @private @const @type {!ProvidersModel} */
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
    // @ts-ignore: error TS2339: Property 'volumeChanged' does not exist on type
    // 'Event'.
    if (event.volumeChanged) {
      this.refreshRemainingSpace_(true);
    }  // Show loading caption.

    // @ts-ignore: error TS2339: Property 'isMenuShown' does not exist on type
    // 'MultiMenuButton'.
    if (this.gearButton_.isMenuShown()) {
      // @ts-ignore: error TS2339: Property 'menu' does not exist on type
      // 'MultiMenuButton'.
      this.gearButton_.menu.updateCommands(this.gearButton_);
    }
  }

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   * @private
   */
  // @ts-ignore: error TS6133: 'showLoadingCaption' is declared but its value is
  // never read.
  refreshRemainingSpace_(showLoadingCaption) {
    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirectory || isRecentRoot(currentDirectory)) {
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'Promise<SpaceInfo | undefined>'.
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
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'Promise<SpaceInfo | undefined>'.
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    if (currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.DRIVE) {
      this.gearMenu_.setSpaceInfo(
          // @ts-ignore: error TS2345: Argument of type 'Promise<SpaceInfo | {
          // totalSize: number; usedSize: number; warningMessage: string | null;
          // } | undefined>' is not assignable to parameter of type
          // 'Promise<SpaceInfo | undefined>'.
          getDriveQuotaMetadata(currentDirectory)
              .then(
                  quota /* chrome.fileManagerPrivate.DriveQuotaMetadata */ => ({
                    // @ts-ignore: error TS18048: 'quota' is possibly
                    // 'undefined'.
                    totalSize: quota.totalBytes,
                    // @ts-ignore: error TS18048: 'quota' is possibly
                    // 'undefined'.
                    usedSize: quota.usedBytes,
                    // @ts-ignore: error TS18048: 'quota' is possibly
                    // 'undefined'.
                    warningMessage: quota.organizationLimitExceeded ?
                        str('DRIVE_ORGANIZATION_STORAGE_FULL') :
                        null,
                  })),
          true);
      return;
    }

    this.gearMenu_.setSpaceInfo(
        // @ts-ignore: error TS2345: Argument of type 'Promise<SpaceInfo | {
        // totalSize: number; usedSize: number; } | undefined>' is not
        // assignable to parameter of type 'Promise<SpaceInfo | undefined>'.
        getSizeStats(currentVolumeInfo.volumeId)
            .then(
                size /* chrome.fileManagerPrivate.MountPointSizeStats */ => ({
                  // @ts-ignore: error TS18048: 'size' is possibly 'undefined'.
                  totalSize: size.totalSize,
                  // @ts-ignore: error TS18048: 'size' is possibly 'undefined'.
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

      if (prefs.driveSyncEnabledOnMeteredNetwork) {
        this.gearMenu_.syncButton.setAttribute('checked', '');
      } else {
        this.gearMenu_.syncButton.removeAttribute('checked');
      }
    });
  }
}
