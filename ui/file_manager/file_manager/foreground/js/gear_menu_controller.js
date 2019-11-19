// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class GearMenuController {
  /**
   * @param {!cr.ui.MultiMenuButton} gearButton
   * @param {!FilesToggleRipple} toggleRipple
   * @param {!GearMenu} gearMenu
   * @param {!ProvidersMenu} providersMenu
   * @param {!DirectoryModel} directoryModel
   * @param {!CommandHandler} commandHandler
   * @param {!ProvidersModel} providersModel
   */
  constructor(
      gearButton, toggleRipple, gearMenu, providersMenu, directoryModel,
      commandHandler, providersModel) {
    /** @private @const {!cr.ui.MultiMenuButton} */
    this.gearButton_ = gearButton;

    /** @private @const {!FilesToggleRipple} */
    this.toggleRipple_ = toggleRipple;

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
    gearButton.addEventListener('menuhide', this.onHideGearMenu_.bind(this));
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
    this.toggleRipple_.activated = true;
    this.refreshRemainingSpace_(false); /* Without loading caption. */
    this.updateNewServiceItem();
  }

  /**
   * Update "New service" menu item to either directly show the Webstore dialog
   * when there isn't any service/FSP extension installed, or display the
   * providers menu with the currently installed extensions and also install new
   * service.
   *
   * @private
   */
  updateNewServiceItem() {
    this.providersModel_.getMountableProviders().then(providers => {
      // Go straight to webstore to install the first provider.
      let desiredMenu = '#install-new-extension';
      let label = str('INSTALL_NEW_EXTENSION_LABEL');

      const shouldDisplayProvidersMenu = providers.length > 0;
      if (shouldDisplayProvidersMenu) {
        // Open the providers menu with an installed provider and an install new
        // provider option.
        desiredMenu = '#new-service';
        label = str('ADD_NEW_SERVICES_BUTTON_LABEL');
        // Trigger an update of the providers submenu.
        this.providersMenu_.updateSubMenu();
      }

      this.gearMenu_.setNewServiceCommand(desiredMenu, label);
    });
  }

  /**
   * @private
   */
  onHideGearMenu_() {
    this.toggleRipple_.activated = false;
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
    // TODO(fukino): Add support for remaining space indication for documents
    // provider roots. crbug.com/953657.
    if (currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.PROVIDED ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.MEDIA_VIEW ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.ARCHIVE ||
        currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.SMB) {
      this.gearMenu_.setSpaceInfo(null, false);
      return;
    }

    this.gearMenu_.setSpaceInfo(
        new Promise(fulfill => {
          chrome.fileManagerPrivate.getSizeStats(
              currentVolumeInfo.volumeId, fulfill);
        }),
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
