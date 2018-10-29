// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 *
 * @constructor
 * @implements {VolumeManager}
 * @extends {cr.EventTarget}
 */
function VolumeManagerImpl() {
  /** @override */
  this.volumeInfoList = new VolumeInfoListImpl();

  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   * @type {Object<Object>}
   * @private
   */
  this.requests_ = {};

  /**
   * Queue for mounting.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.mountQueue_ = new AsyncUtil.Queue();

  // The status should be merged into VolumeManager.
  // TODO(hidehiko): Remove them after the migration.
  /**
   * Connection state of the Drive.
   * @type {VolumeManagerCommon.DriveConnectionState}
   * @private
   */
  this.driveConnectionState_ = {
    type: VolumeManagerCommon.DriveConnectionType.OFFLINE,
    reason: VolumeManagerCommon.DriveConnectionReason.NO_SERVICE,
    hasCellularNetworkAccess: false
  };

  chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
      this.onDriveConnectionStatusChanged_.bind(this));
  this.onDriveConnectionStatusChanged_();
}

/**
 * Invoked when the drive connection status is changed.
 * @private
 */
VolumeManagerImpl.prototype.onDriveConnectionStatusChanged_ = function() {
  chrome.fileManagerPrivate.getDriveConnectionState(function(state) {
    this.driveConnectionState_ = state;
    cr.dispatchSimpleEvent(this, 'drive-connection-changed');
  }.bind(this));
};

/** @override */
VolumeManagerImpl.prototype.getDriveConnectionState = function() {
  return this.driveConnectionState_;
};

/**
 * VolumeManager extends cr.EventTarget.
 */
VolumeManagerImpl.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Adds new volume info from the given volumeMetadata. If the corresponding
 * volume info has already been added, the volumeMetadata is ignored.
 * @param {!chrome.fileManagerPrivate.VolumeMetadata} volumeMetadata
 * @return {!Promise<!VolumeInfo>}
 * @private
 */
VolumeManagerImpl.prototype.addVolumeMetadata_ = function(volumeMetadata) {
  return volumeManagerUtil.createVolumeInfo(volumeMetadata).then(
      (/**
       * @param {!VolumeInfo} volumeInfo
       * @return {!VolumeInfo}
       */
      function(volumeInfo) {
        // We don't show Downloads and Drive on volume list if they have mount
        // error, since users can do nothing in this situation.
        // We show Removable and Provided volumes regardless of mount error so
        // that users can unmount or format the volume.
        // TODO(fukino): Once the Files app gets ready, show erroneous Drive
        // volume so that users can see auth warning banner on the volume.
        // crbug.com/517772.
        var shouldShow = true;
        switch (volumeInfo.volumeType) {
          case VolumeManagerCommon.VolumeType.DOWNLOADS:
          case VolumeManagerCommon.VolumeType.DRIVE:
            shouldShow = !!volumeInfo.fileSystem;
            break;
        }
        if (!shouldShow)
            return volumeInfo;
        if (this.volumeInfoList.findIndex(volumeInfo.volumeId) === -1) {
          this.volumeInfoList.add(volumeInfo);

          // Update the network connection status, because until the drive is
          // initialized, the status is set to not ready.
          // TODO(mtomasz): The connection status should be migrated into
          // chrome.fileManagerPrivate.VolumeMetadata.
          if (volumeMetadata.volumeType ===
              VolumeManagerCommon.VolumeType.DRIVE) {
            this.onDriveConnectionStatusChanged_();
          }
        } else if (volumeMetadata.volumeType ===
            VolumeManagerCommon.VolumeType.REMOVABLE) {
          // Update for remounted USB external storage, because they were
          // remounted to switch read-only policy.
          this.volumeInfoList.add(volumeInfo);
        }
        return volumeInfo;
      }).bind(this));
};

/**
 * Initializes mount points.
 * @param {function()} callback Called upon the completion of the
 *     initialization.
 * @private
 */
VolumeManagerImpl.prototype.initialize_ = function(callback) {
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));
  console.debug('Requesting volume list.');
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    console.debug(
        'Volume list fetched with: ' + volumeMetadataList.length + ' items.');
    // We must subscribe to the mount completed event in the callback of
    // getVolumeMetadataList. crbug.com/330061.
    // But volumes reported by onMountCompleted events must be added after the
    // volumes in the volumeMetadataList are mounted. crbug.com/135477.
    this.mountQueue_.run(function(inCallback) {
      // Create VolumeInfo for each volume.
      Promise.all(
          volumeMetadataList.map(function(volumeMetadata) {
            console.debug(
                'Initializing volume: ' + volumeMetadata.volumeId);
            return this.addVolumeMetadata_(volumeMetadata).then(
                function(volumeInfo) {
                  console.debug('Initialized volume: ' + volumeInfo.volumeId);
                });
          }.bind(this)))
          .then(function() {
            console.debug('Initialized all volumes.');
            // Call the callback of the initialize function.
            callback();
            // Call the callback of AsyncQueue. Maybe it invokes callbacks
            // registered by mountCompleted events.
            inCallback();
          });
    }.bind(this));
  }.bind(this));
};

/**
 * Event handler called when some volume was mounted or unmounted.
 * @param {chrome.fileManagerPrivate.MountCompletedEvent} event Received event.
 * @private
 */
VolumeManagerImpl.prototype.onMountCompleted_ = function(event) {
  this.mountQueue_.run(function(callback) {
    switch (event.eventType) {
      case 'mount':
        var requestKey = this.makeRequestKey_(
            'mount',
            event.volumeMetadata.sourcePath || '');

        if (event.status === 'success' ||
            event.status ===
                VolumeManagerCommon.VolumeError.UNKNOWN_FILESYSTEM ||
            event.status ===
                VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
          this.addVolumeMetadata_(event.volumeMetadata).then(
              function(volumeInfo) {
                this.finishRequest_(requestKey, event.status, volumeInfo);
                callback();
              }.bind(this));
        } else if (event.status ===
            VolumeManagerCommon.VolumeError.ALREADY_MOUNTED) {
          var navigationEvent =
              new Event(VolumeManagerCommon.VOLUME_ALREADY_MOUNTED);
          navigationEvent.volumeId = event.volumeMetadata.volumeId;
          this.dispatchEvent(navigationEvent);
          this.finishRequest_(requestKey, event.status, volumeInfo);
          callback();
        } else {
          console.warn('Failed to mount a volume: ' + event.status);
          this.finishRequest_(requestKey, event.status);
          callback();
        }
        break;

      case 'unmount':
        var volumeId = event.volumeMetadata.volumeId;
        var status = event.status;
        var requestKey = this.makeRequestKey_('unmount', volumeId);
        var requested = requestKey in this.requests_;
        var volumeInfoIndex =
            this.volumeInfoList.findIndex(volumeId);
        var volumeInfo = volumeInfoIndex !== -1 ?
            this.volumeInfoList.item(volumeInfoIndex) : null;
        if (event.status === 'success' && !requested && volumeInfo) {
          console.warn('Unmounted volume without a request: ' + volumeId);
          var e = new Event('externally-unmounted');
          e.volumeInfo = volumeInfo;
          this.dispatchEvent(e);
        }

        this.finishRequest_(requestKey, status);
        if (event.status === 'success')
          this.volumeInfoList.remove(event.volumeMetadata.volumeId);
        console.debug('unmounted volume: ' + volumeId);
        callback();
        break;
    }
  }.bind(this));
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by
 *     enum.
 * @param {string} argument Argument describing the request, eg. source file
 *     path of the archive to be mounted, or a volumeId for unmounting.
 * @return {string} Key for |this.requests_|.
 * @private
 */
VolumeManagerImpl.prototype.makeRequestKey_ = function(requestType, argument) {
  return requestType + ':' + argument;
};

/** @override */
VolumeManagerImpl.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  chrome.fileManagerPrivate.addMount(fileUrl, function(sourcePath) {
    console.info(
        'Mount request: url=' + fileUrl + '; sourcePath=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/** @override */
VolumeManagerImpl.prototype.unmount = function(volumeInfo,
                                           successCallback,
                                           errorCallback) {
  chrome.fileManagerPrivate.removeMount(volumeInfo.volumeId);
  var requestKey = this.makeRequestKey_('unmount', volumeInfo.volumeId);
  this.startRequest_(requestKey, successCallback, errorCallback);
};

/** @override */
VolumeManagerImpl.prototype.configure = function(volumeInfo) {
  return new Promise(function(fulfill, reject) {
    chrome.fileManagerPrivate.configureVolume(
        volumeInfo.volumeId,
        function() {
          if (chrome.runtime.lastError)
            reject(chrome.runtime.lastError.message);
          else
            fulfill();
        });
  });
};

/** @override */
VolumeManagerImpl.prototype.getVolumeInfo = function(entry) {
  for (let i = 0; i < this.volumeInfoList.length; i++) {
    const volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.fileSystem &&
        util.isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
      return volumeInfo;
    }
    // Additionally, check fake entries.
    for (let key in volumeInfo.fakeEntries_) {
      const fakeEntry = volumeInfo.fakeEntries_[key];
      if (util.isSameEntry(fakeEntry, entry))
        return volumeInfo;
    }
  }
  return null;
};

/** @override */
VolumeManagerImpl.prototype.getCurrentProfileVolumeInfo = function(volumeType) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    var volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.profile.isCurrentProfile &&
        volumeInfo.volumeType === volumeType)
      return volumeInfo;
  }
  return null;
};

/** @override */
VolumeManagerImpl.prototype.getLocationInfo = function(entry) {
  var volumeInfo = this.getVolumeInfo(entry);

  if (util.isFakeEntry(entry)) {
    return new EntryLocationImpl(
        volumeInfo, assert(entry.rootType),
        true /* the entry points a root directory. */,
        true /* fake entries are read only. */);
  }

  if (!volumeInfo)
    return null;

  var rootType;
  var isReadOnly;
  var isRootEntry;
  if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
    // For Drive, the roots are /root, /team_drives, /Computers and /other,
    // instead of /. Root URLs contain trailing slashes.
    if (entry.fullPath == '/root' || entry.fullPath.indexOf('/root/') === 0) {
      rootType = VolumeManagerCommon.RootType.DRIVE;
      isReadOnly = volumeInfo.isReadOnly;
      isRootEntry = entry.fullPath === '/root';
    } else if (
        entry.fullPath == VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH ||
        entry.fullPath.indexOf(
            VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH + '/') === 0) {
      if (entry.fullPath == VolumeManagerCommon.TEAM_DRIVES_DIRECTORY_PATH) {
        rootType = VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT;
        isReadOnly = true;
        isRootEntry = true;
      } else {
        rootType = VolumeManagerCommon.RootType.TEAM_DRIVE;
        if (util.isTeamDriveRoot(entry)) {
          isReadOnly = false;
          isRootEntry = true;
        } else {
          // Regular files/directories under Team Drives.
          isRootEntry = false;
          isReadOnly = volumeInfo.isReadOnly;
        }
      }
    } else if (
        entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH ||
        entry.fullPath.indexOf(
            VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH + '/') === 0) {
      if (entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH) {
        rootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
        isReadOnly = true;
        isRootEntry = true;
      } else {
        rootType = VolumeManagerCommon.RootType.COMPUTER;
        if (util.isComputersRoot(entry)) {
          isReadOnly = false;
          isRootEntry = true;
        } else {
          // Regular files/directories under a Computer entry.
          isRootEntry = false;
          isReadOnly = volumeInfo.isReadOnly;
        }
      }
    } else if (entry.fullPath == '/other' ||
               entry.fullPath.indexOf('/other/') === 0) {
      rootType = VolumeManagerCommon.RootType.DRIVE_OTHER;
      isReadOnly = true;
      isRootEntry = entry.fullPath === '/other';
    } else if (
        entry.fullPath === '/.files-by-id' ||
        entry.fullPath.indexOf('/.files-by-id/') === 0) {
      rootType = VolumeManagerCommon.RootType.DRIVE_OTHER;

      // /.files-by-id/<id> is read-only, but /.files-by-id/<id>/foo is
      // read-write.
      isReadOnly = entry.fullPath.split('/').length < 4;
      isRootEntry = entry.fullPath === '/.files-by-id';
    } else {
      // Accessing Drive files outside of /drive/root and /drive/other is not
      // allowed, but can happen. Therefore returning null.
      return null;
    }
  } else {
    rootType =
        VolumeManagerCommon.getRootTypeFromVolumeType(volumeInfo.volumeType);
    isRootEntry = util.isSameEntry(entry, volumeInfo.fileSystem.root);
    // Although "Play files" root directory is writable in file system level,
    // we prohibit write operations on it in the UI level to avoid confusion.
    // Users can still have write access in sub directories like
    // /Play files/Pictures, /Play files/DCIM, etc...
    if (volumeInfo.volumeType == VolumeManagerCommon.VolumeType.ANDROID_FILES &&
        isRootEntry) {
      isReadOnly = true;
    } else {
      isReadOnly = volumeInfo.isReadOnly;
    }
  }

  return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, isReadOnly);
};

/** @override */
VolumeManagerImpl.prototype.findByDevicePath = function(devicePath) {
  for (let i = 0; i < this.volumeInfoList.length; i++) {
    const volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath)
      return volumeInfo;
  }
  return null;
};

/** @override */
VolumeManagerImpl.prototype.whenVolumeInfoReady = function(volumeId) {
  return new Promise((fulfill) => {
    const handler = () => {
      const index = this.volumeInfoList.findIndex(volumeId);
      if (index !== -1) {
        fulfill(this.volumeInfoList.item(index));
        this.volumeInfoList.removeEventListener('splice', handler);
      }
    };
    this.volumeInfoList.addEventListener('splice', handler);
    handler();
  });
};

/** @override */
VolumeManagerImpl.prototype.getDefaultDisplayRoot = function(callback) {
  console.error('Unexpectedly called VolumeManagerImpl.getDefaultDisplayRoot.');
  callback(null);
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function(VolumeInfo)} successCallback To be called when the request
 *     finishes successfully.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback To be called
 *     when the request fails.
 * @private
 */
VolumeManagerImpl.prototype.startRequest_ = function(key,
    successCallback, errorCallback) {
  if (key in this.requests_) {
    var request = this.requests_[key];
    request.successCallbacks.push(successCallback);
    request.errorCallbacks.push(errorCallback);
  } else {
    this.requests_[key] = {
      successCallbacks: [successCallback],
      errorCallbacks: [errorCallback],

      timeout: setTimeout(this.onTimeout_.bind(this, key),
                          volumeManagerUtil.TIMEOUT)
    };
  }
};

/**
 * Called if no response received in |TIMEOUT|.
 * @param {string} key Key produced by |makeRequestKey_|.
 * @private
 */
VolumeManagerImpl.prototype.onTimeout_ = function(key) {
  this.invokeRequestCallbacks_(this.requests_[key],
                               VolumeManagerCommon.VolumeError.TIMEOUT);
  delete this.requests_[key];
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {VolumeManagerCommon.VolumeError|string} status Status received
 *     from the API.
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManagerImpl.prototype.finishRequest_ =
    function(key, status, opt_volumeInfo) {
  var request = this.requests_[key];
  if (!request)
    return;

  clearTimeout(request.timeout);
  this.invokeRequestCallbacks_(request, status, opt_volumeInfo);
  delete this.requests_[key];
};

/**
 * @param {Object} request Structure created in |startRequest_|.
 * @param {VolumeManagerCommon.VolumeError|string} status If status ===
 *     'success' success callbacks are called.
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManagerImpl.prototype.invokeRequestCallbacks_ = function(
    request, status, opt_volumeInfo) {
  var callEach = function(callbacks, self, args) {
    for (var i = 0; i < callbacks.length; i++) {
      callbacks[i].apply(self, args);
    }
  };
  if (status === 'success') {
    callEach(request.successCallbacks, this, [opt_volumeInfo]);
  } else {
    volumeManagerUtil.validateError(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};
