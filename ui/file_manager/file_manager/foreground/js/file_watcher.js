// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Watches for changes in the tracked directory.
 *
 * @extends {cr.EventTarget}
 * @constructor
 */
function FileWatcher() {
  this.queue_ = new AsyncUtil.Queue();
  this.watchedDirectoryEntry_ = null;

  this.onDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(
      this.onDirectoryChangedBound_);
}

/**
 * FileWatcher extends cr.EventTarget.
 */
FileWatcher.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Stops watching (must be called before page unload).
 */
FileWatcher.prototype.dispose = function() {
  chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
      this.onDirectoryChangedBound_);
  if (this.watchedDirectoryEntry_)
    this.resetWatchedEntry_();
};

/**
 * Called when a file in the watched directory is changed.
 * @param {chrome.fileManagerPrivate.FileWatchEvent} event Change event.
 * @private
 */
FileWatcher.prototype.onDirectoryChanged_ = function(event) {
  var fireWatcherDirectoryChanged = function(changedFiles) {
    var e = new Event('watcher-directory-changed');

    if (changedFiles)
      e.changedFiles = changedFiles;

    this.dispatchEvent(e);
  }.bind(this);

  if (this.watchedDirectoryEntry_) {
    var eventURL = event.entry.toURL();
    var watchedDirURL = this.watchedDirectoryEntry_.toURL();

    if (eventURL === watchedDirURL) {
      fireWatcherDirectoryChanged(event.changedFiles);
    } else if (watchedDirURL.match(new RegExp('^' + eventURL))) {
      // When watched directory is deleted by the change in parent directory,
      // notify it as watcher directory changed.
      this.watchedDirectoryEntry_.getDirectory(
          this.watchedDirectoryEntry_.fullPath,
          {create: false},
          null,
          function() { fireWatcherDirectoryChanged(null); });
    }
  }
};

/**
 * Changes the watched directory. In case of a fake entry, the watch is
 * just released, since there is no reason to track a fake directory.
 *
 * @param {!DirectoryEntry|!FilesAppEntry} entry Directory entry to be tracked,
 *     or the fake entry.
 * @return {!Promise}
 */
FileWatcher.prototype.changeWatchedDirectory = function(entry) {
  if (!util.isFakeEntry(entry))
    return this.changeWatchedEntry_(/** @type {!DirectoryEntry} */ (entry));
  else
    return this.resetWatchedEntry_();
};

/**
 * Resets the watched entry. It's a best effort method.
 * @return {!Promise}
 * @private
 */
FileWatcher.prototype.resetWatchedEntry_ = function() {
  // Run the tasks in the queue to avoid races.
  return new Promise(function(fulfill, reject) {
    this.queue_.run(function(callback) {
      // Release the watched directory.
      if (this.watchedDirectoryEntry_) {
        chrome.fileManagerPrivate.removeFileWatch(
            this.watchedDirectoryEntry_,
            function(result) {
              if (chrome.runtime.lastError) {
                console.error('Failed to remove the watcher because of: ' +
                    chrome.runtime.lastError.message);
              }
              // Even on error reset the watcher locally, so at least the
              // notifications are discarded.
              this.watchedDirectoryEntry_ = null;
              fulfill();
              callback();
            }.bind(this));
      } else {
        fulfill();
        callback();
      }
    }.bind(this));
  }.bind(this));
};

/**
 * Sets the watched entry to the passed directory. It's a best effort method.
 * @param {!DirectoryEntry} entry Directory to be watched.
 * @return {!Promise}
 * @private
 */
FileWatcher.prototype.changeWatchedEntry_ = function(entry) {
  return new Promise(function(fulfill, reject) {
    var setEntryClosure = function() {
      // Run the tasks in the queue to avoid races.
      this.queue_.run(function(callback) {
        chrome.fileManagerPrivate.addFileWatch(
            entry,
            function(result) {
              if (chrome.runtime.lastError) {
                // Most probably setting the watcher is not supported on the
                // file system type.
                console.info('File watchers not supported for: ' +
                    entry.toURL());
                this.watchedDirectoryEntry_ = null;
                fulfill();
              } else {
                this.watchedDirectoryEntry_ = assert(entry);
                fulfill();
              }
              callback();
            }.bind(this));
      }.bind(this));
    }.bind(this);

    // Reset the watched directory first, then set the new watched directory.
    return this.resetWatchedEntry_().then(setEntryClosure);
  }.bind(this));
};

/**
 * @return {DirectoryEntry} Current watched directory entry.
 */
FileWatcher.prototype.getWatchedDirectoryEntry = function() {
  return this.watchedDirectoryEntry_;
};
