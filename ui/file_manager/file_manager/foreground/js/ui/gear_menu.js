// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class GearMenu {
  /**
   * @param {!HTMLElement} element
   */
  constructor(element) {
    /**
     * @type {!HTMLMenuItemElement}
     * @const
     */
    this.syncButton = /** @type {!HTMLMenuItemElement} */
        (queryRequiredElement('#gear-menu-drive-sync-settings', element));

    /**
     * @type {!HTMLElement}
     * @const
     */
    this.volumeSpaceInfo = queryRequiredElement('#volume-space-info', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoSeparator_ =
        queryRequiredElement('#volume-space-info-separator', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInfoLabel_ =
        queryRequiredElement('#volume-space-info-label', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceInnerBar_ =
        queryRequiredElement('#volume-space-info-bar', element);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.volumeSpaceOuterBar_ =
        assertInstanceof(this.volumeSpaceInnerBar_.parentElement, HTMLElement);

    /**
     * @type {!HTMLElement}
     * @const
     * @private
     */
    this.newServiceMenuItem_ =
        queryRequiredElement('#gear-menu-newservice', element);

    /**
     * Volume space info.
     * @type {Promise<chrome.fileManagerPrivate.MountPointSizeStats>}
     * @private
     */
    this.spaceInfoPromise_ = null;

    // Initialize attributes.
    this.syncButton.checkable = true;
  }

  /**
   * @param {!string} commandId Element id of the command that new service menu
   *     should trigger.
   * @param {!string} label Text that should be displayed to user in the menu.
   */
  setNewServiceCommand(commandId, label) {
    this.newServiceMenuItem_.textContent = label;
    // Only change command if needed because it does some parsing when setting.
    if ('#' + this.newServiceMenuItem_.command.id === commandId) {
      return;
    }
    this.newServiceMenuItem_.command = commandId;
  }

  /**
   * @param {Promise<chrome.fileManagerPrivate.MountPointSizeStats>}
   * spaceInfoPromise Promise to be fulfilled with space info.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   */
  setSpaceInfo(spaceInfoPromise, showLoadingCaption) {
    this.spaceInfoPromise_ = spaceInfoPromise;

    if (!spaceInfoPromise || loadTimeData.getBoolean('HIDE_SPACE_INFO')) {
      this.volumeSpaceInfo.hidden = true;
      this.volumeSpaceInfoSeparator_.hidden = true;
      return;
    }

    this.volumeSpaceInfo.hidden = false;
    this.volumeSpaceInfoSeparator_.hidden = false;
    this.volumeSpaceInnerBar_.setAttribute('pending', '');
    if (showLoadingCaption) {
      this.volumeSpaceInfoLabel_.innerText = str('WAITING_FOR_SPACE_INFO');
      this.volumeSpaceInnerBar_.style.width = '100%';
    }

    spaceInfoPromise.then(spaceInfo => {
      if (this.spaceInfoPromise_ != spaceInfoPromise) {
        return;
      }
      this.volumeSpaceInnerBar_.removeAttribute('pending');
      if (spaceInfo) {
        const sizeStr = util.bytesToString(spaceInfo.remainingSize);
        this.volumeSpaceInfoLabel_.textContent =
            strf('SPACE_AVAILABLE', sizeStr);

        const usedSpace = spaceInfo.totalSize - spaceInfo.remainingSize;
        this.volumeSpaceInnerBar_.style.width =
            (100 * usedSpace / spaceInfo.totalSize) + '%';

        this.volumeSpaceOuterBar_.hidden = false;
      } else {
        this.volumeSpaceOuterBar_.hidden = true;
        this.volumeSpaceInfoLabel_.textContent = str('FAILED_SPACE_INFO');
      }
    });
  }
}
