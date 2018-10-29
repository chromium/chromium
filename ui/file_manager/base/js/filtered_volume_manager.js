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
 * @implements {VolumeInfoList}
 */
class FilteredVolumeInfoList {
  /**
   * @param {!cr.ui.ArrayDataModel} list
   */
  constructor(list) {
    /** @private */
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
 * @constructor
 * @extends {cr.EventTarget}
 * @implements {VolumeManager}
 *
 * @param {!AllowedPaths} allowedPaths Which paths are supported in the Files
 *     app dialog.
 * @param {boolean} writableOnly If true, only writable volumes are returned.
 * @param {Window=} opt_backgroundPage Window object of the background
 *     page. If this is specified, the class skips to get background page.
 *     TODO(hirono): Let all clients of the class pass the background page and
 *     make the argument not optional.
 */
function FilteredVolumeManager(allowedPaths, writableOnly, opt_backgroundPage) {
  cr.EventTarget.call(this);

  this.allowedPaths_ = allowedPaths;
  this.writableOnly_ = writableOnly;
  // Internal list holds filtered VolumeInfo instances.
  /** @private */
  this.list_ = new cr.ui.ArrayDataModel([]);
  // Public VolumeManager.volumeInfoList property accessed by callers.
  this.volumeInfoList = new FilteredVolumeInfoList(this.list_);

  this.volumeManager_ = null;
  this.pendingTasks_ = [];
  this.onEventBound_ = this.onEvent_.bind(this);
  this.onVolumeInfoListUpdatedBound_ =
      this.onVolumeInfoListUpdated_.bind(this);

  this.disposed_ = false;

  // Start initialize the VolumeManager.
  var queue = new AsyncUtil.Queue();

  if (opt_backgroundPage) {
    this.backgroundPage_ = opt_backgroundPage;
  } else {
    queue.run(function(callNextStep) {
      chrome.runtime.getBackgroundPage(/** @type {function(Window=)} */(
          function(opt_backgroundPage) {
            this.backgroundPage_ = opt_backgroundPage;
            callNextStep();
          }.bind(this)));
    }.bind(this));
  }

  queue.run(function(callNextStep) {
    this.backgroundPage_.volumeManagerFactory.getInstance(
        function(volumeManager) {
          this.onReady_(volumeManager);
          callNextStep();
        }.bind(this));
  }.bind(this));
}

/**
 * Extends cr.EventTarget.
 */
FilteredVolumeManager.prototype.__proto__ = cr.EventTarget.prototype;

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
FilteredVolumeManager.prototype.isAllowedVolumeType_ = function(volumeType) {
  switch (this.allowedPaths_) {
    case AllowedPaths.ANY_PATH:
      return true;
    case AllowedPaths.NATIVE_OR_DRIVE_PATH:
      return (VolumeManagerCommon.VolumeType.isNative(volumeType) ||
              volumeType == VolumeManagerCommon.VolumeType.DRIVE);
    case AllowedPaths.NATIVE_PATH:
      return VolumeManagerCommon.VolumeType.isNative(volumeType);
  }
  return false;
};

/**
 * Checks if a volume is allowed.
 *
 * @param {!VolumeInfo} volumeInfo
 * @return {boolean}
 */
FilteredVolumeManager.prototype.isAllowedVolume_ = function(volumeInfo) {
  if (!this.isAllowedVolumeType_(volumeInfo.volumeType))
    return false;
  if (this.writableOnly_ && volumeInfo.isReadOnly)
    return false;
  return true;
};

/**
 * Called when the VolumeManager gets ready for post initialization.
 * @param {VolumeManager} volumeManager The initialized VolumeManager instance.
 * @private
 */
FilteredVolumeManager.prototype.onReady_ = function(volumeManager) {
  if (this.disposed_)
    return;

  this.volumeManager_ = volumeManager;

  // Subscribe to VolumeManager.
  this.volumeManager_.addEventListener(
      'drive-connection-changed', this.onEventBound_);
  this.volumeManager_.addEventListener(
      'externally-unmounted', this.onEventBound_);
  this.volumeManager_.addEventListener(
      VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE, this.onEventBound_);

  // Dispatch 'drive-connection-changed' to listeners, since the return value of
  // FilteredVolumeManager.getDriveConnectionState() can be changed by setting
  // this.volumeManager_.
  cr.dispatchSimpleEvent(this, 'drive-connection-changed');

  // Cache volumeInfoList.
  var volumeInfoList = [];
  for (var i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
    var volumeInfo = this.volumeManager_.volumeInfoList.item(i);
    // TODO(hidehiko): Filter mounted volumes located on Drive File System.
    if (!this.isAllowedVolume_(volumeInfo))
      continue;
    volumeInfoList.push(volumeInfo);
  }
  this.list_.splice.apply(
      this.list_, [0, this.volumeInfoList.length].concat(volumeInfoList));

  // Subscribe to VolumeInfoList.
  // In VolumeInfoList, we only use 'splice' event.
  this.volumeManager_.volumeInfoList.addEventListener(
      'splice', this.onVolumeInfoListUpdatedBound_);

  // Run pending tasks.
  var pendingTasks = this.pendingTasks_;
  this.pendingTasks_ = null;
  for (var i = 0; i < pendingTasks.length; i++)
    pendingTasks[i]();
};

/**
 * Disposes the instance. After the invocation of this method, any other
 * method should not be called.
 */
FilteredVolumeManager.prototype.dispose = function() {
  this.disposed_ = true;

  if (!this.volumeManager_)
    return;
  this.volumeManager_.removeEventListener(
      'drive-connection-changed', this.onEventBound_);
  this.volumeManager_.removeEventListener(
      'externally-unmounted', this.onEventBound_);
  this.volumeManager_.volumeInfoList.removeEventListener(
      'splice', this.onVolumeInfoListUpdatedBound_);
};

/**
 * Called on events sent from VolumeManager. This has responsibility to
 * re-dispatch the event to the listeners.
 * @param {!Event} event Event object sent from VolumeManager.
 * @private
 */
FilteredVolumeManager.prototype.onEvent_ = function(event) {
  switch (event.type) {
    case 'drive-connection-changed':
      if (this.isAllowedVolumeType_(VolumeManagerCommon.VolumeType.DRIVE))
        this.dispatchEvent(event);
      break;
    case 'externally-unmounted':
      event = /** @type {!ExternallyUnmountedEvent} */ (event);
      if (this.isAllowedVolume_(event.volumeInfo))
        this.dispatchEvent(event);
      break;
    case VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE:
      this.dispatchEvent(event);
      break;
  }
};

/**
 * Called on events of modifying VolumeInfoList.
 * @param {Event} event Event object sent from VolumeInfoList.
 * @private
 */
FilteredVolumeManager.prototype.onVolumeInfoListUpdated_ = function(event) {
  // Filters some volumes.
  var index = event.index;
  for (var i = 0; i < event.index; i++) {
    var volumeInfo = this.volumeManager_.volumeInfoList.item(i);
    if (!this.isAllowedVolume_(volumeInfo))
      index--;
  }

  var numRemovedVolumes = 0;
  for (var i = 0; i < event.removed.length; i++) {
    var volumeInfo = event.removed[i];
    if (this.isAllowedVolume_(volumeInfo))
      numRemovedVolumes++;
  }

  var addedVolumes = [];
  for (var i = 0; i < event.added.length; i++) {
    var volumeInfo = event.added[i];
    if (this.isAllowedVolume_(volumeInfo)) {
      addedVolumes.push(volumeInfo);
    }
  }

  this.list_.splice.apply(
      this.list_, [index, numRemovedVolumes].concat(addedVolumes));
};

/**
 * Returns whether the VolumeManager is initialized or not.
 * @return {boolean} True if the VolumeManager is initialized.
 */
FilteredVolumeManager.prototype.isInitialized = function() {
  return this.pendingTasks_ === null;
};

/**
 * Ensures the VolumeManager is initialized, and then invokes callback.
 * If the VolumeManager is already initialized, callback will be called
 * immediately.
 * @param {function()} callback Called on initialization completion.
 */
FilteredVolumeManager.prototype.ensureInitialized = function(callback) {
  if (!this.isInitialized()) {
    this.pendingTasks_.push(this.ensureInitialized.bind(this, callback));
    return;
  }

  callback();
};

/**
 * @return {VolumeManagerCommon.DriveConnectionState} Current drive connection
 *     state.
 */
FilteredVolumeManager.prototype.getDriveConnectionState = function() {
  if (!this.isAllowedVolumeType_(VolumeManagerCommon.VolumeType.DRIVE) ||
      !this.volumeManager_) {
    return {
      type: VolumeManagerCommon.DriveConnectionType.OFFLINE,
      reason: VolumeManagerCommon.DriveConnectionReason.NO_SERVICE
    };
  }

  return this.volumeManager_.getDriveConnectionState();
};

/** @override */
FilteredVolumeManager.prototype.getVolumeInfo = function(entry) {
  return this.filterDisallowedVolume_(
      this.volumeManager_ && this.volumeManager_.getVolumeInfo(entry));
};

/**
 * Obtains a volume information of the current profile.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Found volume info.
 */
FilteredVolumeManager.prototype.getCurrentProfileVolumeInfo =
    function(volumeType) {
  return this.filterDisallowedVolume_(
      this.volumeManager_ &&
      this.volumeManager_.getCurrentProfileVolumeInfo(volumeType));
};

/** @override */
FilteredVolumeManager.prototype.getDefaultDisplayRoot =
    function(callback) {
  this.ensureInitialized(function() {
    var defaultVolume = this.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DOWNLOADS);
    if (defaultVolume) {
      defaultVolume.resolveDisplayRoot(callback, function() {
        // defaultVolume is DOWNLOADS and resolveDisplayRoot should succeed.
        throw new Error(
            'Unexpectedly failed to obtain the default display root.');
      });
    } else {
      console.warn('Unexpectedly failed to obtain the default display root.');
      callback(null);
    }
  }.bind(this));
};

/**
 * Obtains location information from an entry.
 *
 * @param {(!Entry|!FilesAppEntry)} entry File or directory entry.
 * @return {EntryLocation} Location information.
 */
FilteredVolumeManager.prototype.getLocationInfo = function(entry) {
  var locationInfo =
      this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
  if (!locationInfo)
    return null;
  if (locationInfo.volumeInfo &&
      !this.filterDisallowedVolume_(locationInfo.volumeInfo))
    return null;
  return locationInfo;
};

/** @override */
FilteredVolumeManager.prototype.findByDevicePath = function(devicePath) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    const volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath)
      return this.filterDisallowedVolume_(volumeInfo);
  }
  return null;
};

/**
 * Returns a promise that will be resolved when volume info, identified
 * by {@code volumeId} is created.
 *
 * @param {string} volumeId
 * @return {!Promise<!VolumeInfo>} The VolumeInfo. Will not resolve
 *     if the volume is never mounted.
 */
FilteredVolumeManager.prototype.whenVolumeInfoReady = function(volumeId) {
  return new Promise(resolve => {
    this.volumeManager_.whenVolumeInfoReady(volumeId).then((volumeInfo) => {
      volumeInfo = this.filterDisallowedVolume_(volumeInfo);
      if (volumeInfo)
        resolve(volumeInfo);
    });
  });
};

/**
 * Requests to mount the archive file.
 * @param {string} fileUrl The path to the archive file to be mounted.
 * @param {function(VolumeInfo)} successCallback Called with the VolumeInfo
 *     instance.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Called when
 *     an error occurs.
 */
FilteredVolumeManager.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  if (this.pendingTasks_) {
    this.pendingTasks_.push(
        this.mountArchive.bind(this, fileUrl, successCallback, errorCallback));
    return;
  }

  this.volumeManager_.mountArchive(fileUrl, successCallback, errorCallback);
};

/**
 * Requests unmount the specified volume.
 * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
 * @param {function()} successCallback Called on success.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Called when
 *     an error occurs.
 */
FilteredVolumeManager.prototype.unmount = function(
    volumeInfo, successCallback, errorCallback) {
  if (this.pendingTasks_) {
    this.pendingTasks_.push(
        this.unmount.bind(this, volumeInfo, successCallback, errorCallback));
    return;
  }

  this.volumeManager_.unmount(volumeInfo, successCallback, errorCallback);
};

/**
 * Requests configuring of the specified volume.
 * @param {!VolumeInfo} volumeInfo Volume to be configured.
 * @return {!Promise} Fulfilled on success, otherwise rejected with an error
 *     message.
 */
FilteredVolumeManager.prototype.configure = function(volumeInfo) {
  if (this.pendingTasks_) {
    return new Promise(function(fulfill, reject) {
      this.pendingTasks_.push(function() {
        return this.volumeManager_.configure(volumeInfo).then(fulfill, reject);
      }.bind(this));
    }.bind(this));
  }

  return this.volumeManager_.configure(volumeInfo);
};

/**
 * Filters volume info by isAllowedVolume_().
 *
 * @param {VolumeInfo} volumeInfo Volume info.
 * @return {VolumeInfo} Null if the volume is disallowed. Otherwise just returns
 *     the volume.
 * @private
 */
FilteredVolumeManager.prototype.filterDisallowedVolume_ =
    function(volumeInfo) {
  if (volumeInfo && this.isAllowedVolume_(volumeInfo)) {
    return volumeInfo;
  } else {
    return null;
  }
};
