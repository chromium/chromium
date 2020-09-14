// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implementation of VolumeInfoList for FilteredVolumeManager.
 * In foreground/ we want to enforce this list to be filtered, so we forbid
 * adding/removing/splicing of the list.
 * The inner list ownership is shared between FilteredVolumeInfoList and
 * FilteredVolumeManager to enforce these constraints.
 *
 * @final
 * @implements {VolumeInfoList}
 */
class FilteredVolumeInfoList {
  /**
   * @param {!cr.ui.ArrayDataModel} list
   */
  constructor(list) {
    /** @private @const */
    this.list_ = list;
  }

  /** @override */
  get length() {
    return this.list_.length;
  }

  /** @override */
  addEventListener(type, handler) {
    this.list_.addEventListener(type, handler);
  }

  /** @override */
  removeEventListener(type, handler) {
    this.list_.removeEventListener(type, handler);
  }

  /** @override */
  add(volumeInfo) {
    throw new Error('FilteredVolumeInfoList.add not allowed in foreground');
  }

  /** @override */
  remove(volumeInfo) {
    throw new Error('FilteredVolumeInfoList.remove not allowed in foreground');
  }

  /** @override */
  item(index) {
    return /** @type {!VolumeInfo} */ (this.list_.item(index));
  }
}

/**
 * Thin wrapper for VolumeManager. This should be an interface proxy to talk
 * to VolumeManager. This class also filters some "disallowed" volumes;
 * for example, Drive volumes are dropped if Drive is disabled, and read-only
 * volumes are dropped in save-as dialogs.
 *
 * @implements {VolumeManager}
 */
class FilteredVolumeManager extends cr.EventTarget {
  /**
   *
   * @param {!AllowedPaths} allowedPaths Which paths are supported in the Files
   *     app dialog.
   * @param {boolean} writableOnly If true, only writable volumes are returned.
   * @param {Window=} opt_backgroundPage Window object of the background
   *     page. If this is specified, the class skips to get background page.
   *     TODO(hirono): Let all clients of the class pass the background page and
   *     make the argument not optional.
   */
  constructor(allowedPaths, writableOnly, opt_backgroundPage) {
    super();

    this.allowedPaths_ = allowedPaths;
    this.writableOnly_ = writableOnly;
    // Internal list holds filtered VolumeInfo instances.
    /** @private */
    this.list_ = new cr.ui.ArrayDataModel([]);
    // Public VolumeManager.volumeInfoList property accessed by callers.
    this.volumeInfoList = new FilteredVolumeInfoList(this.list_);

    /** @private {?VolumeManager} */
    this.volumeManager_ = null;

    this.onEventBound_ = this.onEvent_.bind(this);
    this.onVolumeInfoListUpdatedBound_ =
        this.onVolumeInfoListUpdated_.bind(this);

    this.disposed_ = false;

    /** private {Window} */
    this.backgroundPage_ = opt_backgroundPage;

    /**
     * Tracks async initialization of volume manager.
     * @private @const {!Promise<void> }
     */
    this.initialized_ = this.initialize_();
  }

  /**
   * Checks if a volume type is allowed.
   *
   * Note that even if a volume type is allowed, a volume of that type might be
   * disallowed for other restrictions. To check if a specific volume is allowed
   * or not, use isAllowedVolume_() instead.
   *
   * @param {VolumeManagerCommon.VolumeType} volumeType
   * @return {boolean}
   */
  isAllowedVolumeType_(volumeType) {
    switch (this.allowedPaths_) {
      case AllowedPaths.ANY_PATH:
      case AllowedPaths.ANY_PATH_OR_URL:
        return true;
      case AllowedPaths.NATIVE_PATH:
        return VolumeManagerCommon.VolumeType.isNative(volumeType);
    }
    return false;
  }

  /**
   * Checks if a volume is allowed.
   *
   * @param {!VolumeInfo} volumeInfo
   * @return {boolean}
   */
  isAllowedVolume_(volumeInfo) {
    if (!this.isAllowedVolumeType_(volumeInfo.volumeType)) {
      return false;
    }
    if (this.writableOnly_ && volumeInfo.isReadOnly) {
      return false;
    }
    return true;
  }

  /**
   * Async part of the initialization.
   * @private
   */
  async initialize_() {
    if (!this.backgroundPage_) {
      this.backgroundPage_ = await new Promise(
          resolve => chrome.runtime.getBackgroundPage(resolve));
    }

    this.volumeManager_ =
        await this.backgroundPage_.volumeManagerFactory.getInstance();

    if (this.disposed_) {
      return;
    }

    // Subscribe to VolumeManager.
    this.volumeManager_.addEventListener(
        'drive-connection-changed', this.onEventBound_);
    this.volumeManager_.addEventListener(
        'externally-unmounted', this.onEventBound_);
    this.volumeManager_.addEventListener(
        VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE, this.onEventBound_);

    // Dispatch 'drive-connection-changed' to listeners, since the return value
    // of FilteredVolumeManager.getDriveConnectionState() can be changed by
    // setting this.volumeManager_.
    cr.dispatchSimpleEvent(this, 'drive-connection-changed');

    // Cache volumeInfoList.
    const volumeInfoList = [];
    for (let i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeManager_.volumeInfoList.item(i);
      // TODO(hidehiko): Filter mounted volumes located on Drive File System.
      if (!this.isAllowedVolume_(volumeInfo)) {
        continue;
      }
      volumeInfoList.push(volumeInfo);
    }
    this.list_.splice.apply(
        this.list_, [0, this.volumeInfoList.length].concat(volumeInfoList));

    // Subscribe to VolumeInfoList.
    // In VolumeInfoList, we only use 'splice' event.
    this.volumeManager_.volumeInfoList.addEventListener(
        'splice', this.onVolumeInfoListUpdatedBound_);
  }

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose() {
    this.disposed_ = true;

    if (!this.volumeManager_) {
      return;
    }
    // TODO(crbug.com/972849): Consider using EventTracker instead.
    this.volumeManager_.removeEventListener(
        'drive-connection-changed', this.onEventBound_);
    this.volumeManager_.removeEventListener(
        'externally-unmounted', this.onEventBound_);
    this.volumeManager_.volumeInfoList.removeEventListener(
        'splice', this.onVolumeInfoListUpdatedBound_);
  }

  /**
   * Called on events sent from VolumeManager. This has responsibility to
   * re-dispatch the event to the listeners.
   * @param {!Event} event Event object sent from VolumeManager.
   * @private
   */
  onEvent_(event) {
    // Note: Can not re-dispatch the same |event| object, because it throws a
    // runtime "The event is already being dispatched." error.
    switch (event.type) {
      case 'drive-connection-changed':
        if (this.isAllowedVolumeType_(VolumeManagerCommon.VolumeType.DRIVE)) {
          cr.dispatchSimpleEvent(this, 'drive-connection-changed');
        }
        break;
      case 'externally-unmounted':
        event = /** @type {!ExternallyUnmountedEvent} */ (event);
        if (this.isAllowedVolume_(event.detail)) {
          this.dispatchEvent(
              new CustomEvent('externally-unmount', {detail: event.detail}));
        }
        break;
      case VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE:
        this.dispatchEvent(new CustomEvent(
            VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE,
            {detail: event.detail}));
        break;
    }
  }

  /**
   * Called on events of modifying VolumeInfoList.
   * @param {Event} event Event object sent from VolumeInfoList.
   * @private
   */
  onVolumeInfoListUpdated_(event) {
    // Filters some volumes.
    let index = event.index;
    for (let i = 0; i < event.index; i++) {
      const volumeInfo = this.volumeManager_.volumeInfoList.item(i);
      if (!this.isAllowedVolume_(volumeInfo)) {
        index--;
      }
    }

    let numRemovedVolumes = 0;
    for (let i = 0; i < event.removed.length; i++) {
      const volumeInfo = event.removed[i];
      if (this.isAllowedVolume_(volumeInfo)) {
        numRemovedVolumes++;
      }
    }

    const addedVolumes = [];
    for (let i = 0; i < event.added.length; i++) {
      const volumeInfo = event.added[i];
      if (this.isAllowedVolume_(volumeInfo)) {
        addedVolumes.push(volumeInfo);
      }
    }

    this.list_.splice.apply(
        this.list_, [index, numRemovedVolumes].concat(addedVolumes));
  }

  /**
   * Ensures the VolumeManager is initialized, and then invokes callback.
   * If the VolumeManager is already initialized, callback will be called
   * immediately.
   * @param {function()} callback Called on initialization completion.
   */
  ensureInitialized(callback) {
    this.initialized_.then(callback);
  }

  /**
   * @return {chrome.fileManagerPrivate.DriveConnectionState} Current drive
   *     connection state.
   */
  getDriveConnectionState() {
    if (!this.isAllowedVolumeType_(VolumeManagerCommon.VolumeType.DRIVE) ||
        !this.volumeManager_) {
      return {
        type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
        reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_SERVICE,
        hasCellularNetworkAccess: false,
        canPinHostedFiles: false,
      };
    }

    return this.volumeManager_.getDriveConnectionState();
  }

  /** @override */
  getVolumeInfo(entry) {
    return this.filterDisallowedVolume_(
        this.volumeManager_ && this.volumeManager_.getVolumeInfo(entry));
  }

  /**
   * Obtains a volume information of the current profile.
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {?VolumeInfo} Found volume info.
   */
  getCurrentProfileVolumeInfo(volumeType) {
    return this.filterDisallowedVolume_(
        this.volumeManager_ &&
        this.volumeManager_.getCurrentProfileVolumeInfo(volumeType));
  }

  /** @override */
  getDefaultDisplayRoot(callback) {
    this.ensureInitialized(() => {
      const defaultVolume = this.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DOWNLOADS);
      if (!defaultVolume) {
        console.warn('Cannot get default display root');
        callback(null);
        return;
      }

      defaultVolume.resolveDisplayRoot(callback, () => {
        // defaultVolume is DOWNLOADS and resolveDisplayRoot should succeed.
        console.error('Cannot resolve default display root');
        callback(null);
      });
    });
  }

  /**
   * Obtains location information from an entry.
   *
   * @param {(!Entry|!FilesAppEntry)} entry File or directory entry.
   * @return {?EntryLocation} Location information.
   */
  getLocationInfo(entry) {
    const locationInfo =
        this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
    if (!locationInfo) {
      return null;
    }
    if (locationInfo.volumeInfo &&
        !this.filterDisallowedVolume_(locationInfo.volumeInfo)) {
      return null;
    }
    return locationInfo;
  }

  /** @override */
  findByDevicePath(devicePath) {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath) {
        return this.filterDisallowedVolume_(volumeInfo);
      }
    }
    return null;
  }

  /**
   * Returns a promise that will be resolved when volume info, identified
   * by {@code volumeId} is created.
   *
   * @param {string} volumeId
   * @return {!Promise<!VolumeInfo>} The VolumeInfo. Will not resolve
   *     if the volume is never mounted.
   */
  async whenVolumeInfoReady(volumeId) {
    await this.initialized_;

    const volumeInfo = this.filterDisallowedVolume_(
        await this.volumeManager_.whenVolumeInfoReady(volumeId));

    if (!volumeInfo) {
      throw new Error(`Volume not allowed: ${volumeId}`);
    }

    return volumeInfo;
  }

  /** @override */
  async mountArchive(fileUrl, password) {
    await this.initialized_;
    return this.volumeManager_.mountArchive(fileUrl, password);
  }

  /** @override */
  async unmount(volumeInfo) {
    await this.initialized_;
    return this.volumeManager_.unmount(volumeInfo);
  }

  /**
   * Requests configuring of the specified volume.
   * @param {!VolumeInfo} volumeInfo Volume to be configured.
   * @return {!Promise} Fulfilled on success, otherwise rejected with an error
   *     message.
   */
  async configure(volumeInfo) {
    await this.initialized_;
    return this.volumeManager_.configure(volumeInfo);
  }

  /**
   * Filters volume info by isAllowedVolume_().
   *
   * @param {?VolumeInfo} volumeInfo Volume info.
   * @return {?VolumeInfo} Null if the volume is disallowed. Otherwise just
   *     returns the volume.
   * @private
   */
  filterDisallowedVolume_(volumeInfo) {
    if (volumeInfo && this.isAllowedVolume_(volumeInfo)) {
      return volumeInfo;
    } else {
      return null;
    }
  }
}
