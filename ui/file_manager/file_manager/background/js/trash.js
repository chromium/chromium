// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Files App trash implementation based on
 * https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
 */

/**
 * Wrapper for /.Trash/files and /.Trash/info directories.
 */
class TrashDirs {
  /**
   * @param {!DirectoryEntry} files /.Trash/files directory entry.
   * @param {!DirectoryEntry} info /.Trash/info directory entry.
   */
  constructor(files, info) {
    this.files = files;
    this.info = info;
  }
}

/**
 * Configuration for where Trash is stored in a volume.
 */
class TrashConfig {
  /**
   * @param {VolumeManagerCommon.RootType} rootType
   * @param {string} topDir Top directory of volume. Must end with a slash to
   *     make comparisons simpler.
   * @param {string} trashDir Trash directory. Must end with a slash to make
   *     comparisons simpler.
   * @param {boolean=} prefixPathWithRemoteMount Optional, if true, 'Path=' in
   *     *.trashinfo is prefixed with the volume.remoteMountPath. For crostini,
   *     this is the user's homedir (/home/<username>).
   */
  constructor(rootType, topDir, trashDir, prefixPathWithRemoteMount = false) {
    this.id = `${rootType}-${topDir}`;
    this.rootType = rootType;
    this.topDir = topDir;
    this.trashDir = trashDir;
    this.prefixPathWithRemoteMount = prefixPathWithRemoteMount;
    this.pathPrefix = '';
  }
}

/**
 * Result from calling Trash.removeFileOrDirectory().
 */
class TrashItem {
  /**
   * @param {string} name Name of the file deleted.
   * @param {!Entry} filesEntry Trash files entry.
   * @param {!FileEntry} infoEntry Trash info entry.
   * @param {string=} pathPrefix Optional prefix for 'Path=' in *.trashinfo. For
   *     crostini, this is the user's homedir (/home/<username>).
   */
  constructor(name, filesEntry, infoEntry, pathPrefix = '') {
    this.name = name;
    this.filesEntry = filesEntry;
    this.infoEntry = infoEntry;
    this.pathPrefix = pathPrefix;
  }
}

/**
 * Implementation of trash.
 */
class Trash {
  constructor() {
    /**
     * Store TrashDirs to avoid repeated lookup.
     * @private {!Object<string, !TrashDirs>}
     * @const
     */
    this.trashDirs_ = {};

    /**
     * Set of in-progress deletes. Items in this list are ignored by
     * removeOldItems_(). Use getInProgressKey_() to create a globally unique
     * key.
     *
     * @private {!Set<string>}
     * @const
     */
    this.inProgress_ = new Set();
  }

  /**
   * Only move to trash if feature is on, and entry is in one of the supported
   * volumes, but not already in the trash.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Entry} entry The entry to remove.
   * @return {?TrashConfig} Valid TrashConfig if item should be moved to trash,
   *     else null if item should be permanently deleted.
   */
  shouldMoveToTrash(volumeManager, entry) {
    const info = volumeManager.getLocationInfo(entry);
    if (!loadTimeData.getBoolean('FILES_TRASH_ENABLED') || !info) {
      return null;
    }
    const fullPathSlash = entry.fullPath + '/';
    for (const config of Trash.CONFIG) {
      const entryInVolume = fullPathSlash.startsWith(config.topDir);
      if (config.rootType === info.rootType && entryInVolume) {
        if (config.prefixPathWithRemoteMount) {
          config.pathPrefix = info.volumeInfo.remoteMountPath;
        }
        const entryInTrash = fullPathSlash.startsWith(config.trashDir);
        return entryInTrash ? null : config;
      }
    }
    return null;
  }

  /**
   * Remove a file or a directory, either deleting it permanently, or moving it
   * to the trash.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Entry} entry The entry to remove.
   * @param {boolean} permanentlyDelete If true, entry is deleted, else it is
   *     moved to trash.
   * @return {!Promise<!TrashItem|undefined>} Promise which resolves when entry
   *     is removed, rejects with DOMError.
   */
  removeFileOrDirectory(volumeManager, entry, permanentlyDelete) {
    if (!permanentlyDelete) {
      const config = this.shouldMoveToTrash(volumeManager, entry);
      if (config) {
        return this.trashFileOrDirectory_(entry, config).catch(error => {
          console.log(
              ('Error deleting ' + entry.toURL() +
               ', will refresh trashdir and try again'),
              error);
          delete this.trashDirs_[config.id];
          return this.trashFileOrDirectory_(entry, config);
        });
      }
    }
    return this.permanentlyDeleteFileOrDirectory_(entry);
  }

  /**
   * Delete a file or a directory permanently.
   *
   * @param {!Entry} entry The entry to remove.
   * @return {!Promise<void>} Promise which resolves when entry is removed.
   * @private
   */
  permanentlyDeleteFileOrDirectory_(entry) {
    return new Promise((resolve, reject) => {
      if (entry.isDirectory) {
        entry.removeRecursively(resolve, reject);
      } else {
        entry.remove(resolve, reject);
      }
    });
  }

  /**
   * Get trash files and info directories.
   *
   * @param {!Entry} entry The entry to remove.
   * @param {!TrashConfig} config
   * @return {!Promise<!TrashDirs>} Promise which resolves with trash dirs.
   * @private
   */
  async getTrashDirs_(entry, config) {
    let trashDirs = this.trashDirs_[config.id];
    if (trashDirs) {
      return trashDirs;
    }

    let trashRoot = entry.filesystem.root;
    const parts = config.trashDir.split('/');
    for (const part of parts) {
      if (part) {
        trashRoot = await this.getDirectory_(trashRoot, part);
      }
    }
    const trashFiles = await this.getDirectory_(trashRoot, 'files');
    const trashInfo = await this.getDirectory_(trashRoot, 'info');
    trashDirs = new TrashDirs(trashFiles, trashInfo);
    // Check and remove old items max once per session.
    this.removeOldItems_(trashDirs, Date.now());
    this.trashDirs_[config.id] = trashDirs;
    return trashDirs;
  }

  /**
   * Write /.Trash/info/<name>.trashinfo file.
   * Since creating and writing the file requires multiple async operations, we
   * keep a record of which writes are in progress to be ignored by
   * removeOldItems_() if it is running concurrently.
   *
   * @param {!DirectoryEntry} trashInfoDir /.Trash/info directory.
   * @param {string} trashInfoName name of the *.trashinfo file.
   * @param {string} path path to use in *.trashinfo file.
   * @return {!Promise<!FileEntry>}
   * @private
   */
  async writeTrashInfoFile_(trashInfoDir, trashInfoName, path) {
    return new Promise((resolve, reject) => {
      trashInfoDir.getFile(trashInfoName, {create: true}, infoFile => {
        infoFile.createWriter(writer => {
          writer.onwriteend = () => {
            resolve(infoFile);
          };
          writer.onerror = reject;
          const info = `[Trash Info]\nPath=${path}\nDeletionDate=${
              new Date().toISOString()}`;
          writer.write(new Blob([info], {type: 'text/plain'}));
        }, reject);
      }, reject);
    });
  }

  /**
   * Promise wrapper for FileSystemDirectoryEntry.getDirectory().
   *
   * @param {!DirectoryEntry} dirEntry current directory.
   * @param {string} path name of directory within dirEntry.
   * @return {!Promise<!DirectoryEntry>} Promise which resolves with
   *     <dirEntry>/<path>.
   * @private
   */
  getDirectory_(dirEntry, path) {
    return new Promise((resolve, reject) => {
      dirEntry.getDirectory(path, {create: true}, resolve, reject);
    });
  }

  /**
   * Promise wrapper for FileSystemEntry.moveTo().
   *
   * @param {!T} srcEntry source entry to move.
   * @param {!DirectoryEntry} dstDirEntry destination directory.
   * @param {string} name name of entry in destination directory.
   * @return {!Promise<!T>} Promise which resolves with moved entry.
   * @template T
   * @private
   */
  moveTo_(srcEntry, dstDirEntry, name) {
    return new Promise((resolve, reject) => {
      srcEntry.moveTo(dstDirEntry, name, resolve, reject);
    });
  }

  /**
   * Gets a globally unique key to use in the in-progress set.
   *
   * @param {!DirectoryEntry} trashInfoDir /.Trash/info directory.
   * @param {string} trashInfoName name of the *.trashinfo file.
   */
  getInProgressKey_(trashInfoDir, trashInfoName) {
    return `${trashInfoDir.toURL()}/${trashInfoName}`;
  }

  /**
   * Move a file or a directory to the trash.
   *
   * @param {!Entry} entry The entry to remove.
   * @param {!TrashConfig} config trash config for entry.
   * @return {!Promise<!TrashItem>}
   * @private
   */
  async trashFileOrDirectory_(entry, config) {
    const trashDirs = await this.getTrashDirs_(entry, config);
    const name =
        await fileOperationUtil.deduplicatePath(trashDirs.files, entry.name);
    const trashInfoName = `${name}.trashinfo`;

    // Write trashinfo first, then only move file if info write succeeds.
    // If any step fails, the file will be unchanged, and any partial trashinfo
    // file created will be cleaned up when we remove old items.
    const inProgressKey = this.getInProgressKey_(trashDirs.info, trashInfoName);
    this.inProgress_.add(inProgressKey);
    const infoEntry = await this.writeTrashInfoFile_(
        trashDirs.info, trashInfoName, config.pathPrefix + entry.fullPath);
    const filesEntry = await this.moveTo_(entry, trashDirs.files, name);
    this.inProgress_.delete(inProgressKey);
    return new TrashItem(entry.name, filesEntry, infoEntry, config.pathPrefix);
  }

  /**
   * Restores the specified trash item.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!TrashItem} trashItem item in trash.
   * @return {Promise<void>} Promise which resolves when file is restored.
   */
  async restore(volumeManager, trashItem) {
    // Read Path from info entry.
    const file = await new Promise(
        (resolve, reject) => trashItem.infoEntry.file(resolve, reject));
    const text = await file.text();
    const found = text.match(/^Path=(.*)/m);
    if (!found) {
      throw new DOMException(`No Path found to restore in ${
          trashItem.infoEntry.fullPath}, text=${text}`);
    }
    const path = found[1];
    if (!path.startsWith(trashItem.pathPrefix)) {
      throw new DOMException(`Path does not match expected prefix in ${
          trashItem.infoEntry.fullPath}, prefix=${trashItem.pathPrefix}, text=${
          text}`);
    }
    const pathNoLeadingSlash = path.substring(trashItem.pathPrefix.length + 1);
    const parts = pathNoLeadingSlash.split('/');

    // Move to last directory in path, making sure dirs are created if needed.
    let dir = trashItem.filesEntry.filesystem.root;
    for (let i = 0; i < parts.length - 1; i++) {
      dir = await this.getDirectory_(dir, parts[i]);
    }

    // Restore filesEntry first, then remove its trash infoEntry.
    // If any step fails, then either we still have the file in trash with a
    // valid trashinfo, or file is restored and trashinfo will be cleaned up
    // when we remove old items.
    const name =
        await fileOperationUtil.deduplicatePath(dir, parts[parts.length - 1]);
    await this.moveTo_(trashItem.filesEntry, dir, name);
    await this.permanentlyDeleteFileOrDirectory_(trashItem.infoEntry);
  }

  /**
   * Remove any items from trash older than 30d.
   * @param {!TrashDirs} trashDirs
   * @param {number} now Current time in milliseconds from epoch.
   */
  async removeOldItems_(trashDirs, now) {
    const ls = (reader) => {
      return new Promise((resolve, reject) => {
        reader.readEntries(results => resolve(results), error => reject(error));
      });
    };
    const rm = (entry, log, desc) => {
      if (entry) {
        log(`Deleting ${entry.toURL()}: ${desc}`);
        return this.permanentlyDeleteFileOrDirectory_(entry).catch(
            e => console.error(`Error deleting ${entry.toURL()}: ${desc}`, e));
      }
    };

    // Get all entries in trash/files. Read files first before info in case
    // trash or restore operations happen during this.
    const filesEntries = {};
    const filesReader = trashDirs.files.createReader();
    try {
      while (true) {
        const entries = await ls(filesReader);
        if (!entries.length) {
          break;
        }
        entries.forEach(entry => filesEntries[entry.name] = entry);
      }
    } catch (e) {
      console.error('Error reading old files entries', e);
      return;
    }

    // Check entries in trash/info and delete items older than 30d.
    const infoReader = trashDirs.info.createReader();
    try {
      while (true) {
        const entries = await ls(infoReader);
        if (!entries.length) {
          break;
        }
        for (const entry of entries) {
          // Delete any directories.
          if (!entry.isFile) {
            rm(entry, console.error, 'Unexpected trash info directory');
            continue;
          }

          // Delete any files not *.trashinfo.
          if (!entry.name.endsWith('.trashinfo')) {
            rm(entry, console.error, 'Unexpected trash info file');
            continue;
          }

          // Ignore any in-progress files.
          if (this.inProgress_.has(
                  this.getInProgressKey_(trashDirs.info, entry.name))) {
            console.log(`Ignoring write in progress ${entry.toURL()}`);
            continue;
          }

          const name = entry.name.substring(0, entry.name.length - 10);
          const filesEntry = filesEntries[name];
          delete filesEntries[name];

          // Delete any .trashinfo file with no matching file entry (unless it
          // was write-in-progress).
          if (!filesEntry) {
            rm(entry, console.error, 'No matching files entry');
            continue;
          }

          // Delete any entries with no DeletionDate.
          const file = await new Promise(
              (resolve, reject) => entry.file(resolve, reject));
          const text = await file.text();
          const found = text.match(/^DeletionDate=(.*)/m);
          if (!found) {
            rm(entry, console.error, 'Could not find DeletionDate in ' + text);
            rm(filesEntry, console.error, 'Invalid matching trashinfo');
            continue;
          }

          // Delete any entries with invalid DeletionDate.
          const d = Date.parse(found[1]);
          if (!d) {
            rm(entry, console.error, 'Could not parse DeletionDate in ' + text);
            rm(filesEntry, console.error, 'Invalid matching trashinfo');
            continue;
          }

          // Delete entries older than 30d.
          const ago30d = now - Trash.AUTO_DELETE_INTERVAL_MS;
          const ago30dStr = new Date(ago30d).toISOString();
          if (d < ago30d) {
            const msg = `Older than ${ago30dStr}, DeletionDate=${found[1]}`;
            rm(entry, console.log, msg);
            rm(filesEntry, console.log, msg);
          }
        }
      }
    } catch (e) {
      console.error('Error reading old info entries', e);
      return;
    }

    // Any entries left in filesEntries have no matching *.trashinfo file.
    for (const entry of Object.values(filesEntries)) {
      rm(entry, console.error, 'No matching *.trashinfo file');
    }
  }
}

/**
 * Interval (ms) until items in trash are permanently deleted. 30 days.
 * @const
 */
Trash.AUTO_DELETE_INTERVAL_MS = 30 * 24 * 60 * 60 * 1000;

/**
 * Volumes supported for Trash, and location of Trash dir. Items will be
 * searched in order.
 *
 * @type {!Array<!TrashConfig>}
 */
Trash.CONFIG = [
  // MyFiles/Downloads is a separate volume on a physical device, and doing a
  // move from MyFiles/Downloads/<path> to MyFiles/.Trash actually does a
  // copy across volumes, so we have a dedicated MyFiles/Downloads/.Trash.
  new TrashConfig(
      VolumeManagerCommon.RootType.DOWNLOADS, '/Downloads/',
      '/Downloads/.Trash/'),
  new TrashConfig(VolumeManagerCommon.RootType.DOWNLOADS, '/', '/.Trash/'),
  new TrashConfig(
      VolumeManagerCommon.RootType.CROSTINI, '/', '/.local/share/Trash/',
      /*prefixPathWithRemoteMount=*/ true),
];
