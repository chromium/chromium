// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Files App trash implementation based on
 * https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html
 */

import {assert} from 'chrome://resources/js/assert.m.js';

import {AUTO_DELETE_INTERVAL_MS, TrashConfig, TrashDirs, TrashEntry} from '../../common/js/trash.js';
import {util} from '../../common/js/util.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {fileOperationUtil} from './file_operation_util.js';

/**
 * Implementation of trash.
 */
export class Trash {
  constructor() {
    /**
     * Store TrashDirs to avoid repeated lookup, keyed by TrashConfig.id.
     * @private {!Object<string, !TrashDirs>}
     * @const
     */
    this.trashDirs_ = {};

    /**
     * Set of in-progress deletes, keyed by TrashConfig.id with Set containing
     * *.trashinfo filename. Items in this list are ignored by
     * removeOldItems_(). Each Set entry is removed once removeOldItems_() runs
     * for the related TrashConfig.
     *
     * @private {!Map<string, Set<string>>}
     * @const
     */
    this.inProgress_ = new Map(TrashConfig.CONFIG.map(c => [c.id, new Set()]));
  }

  /**
   * Get the TrashConfig for the trash that this entry would be placed in, if
   * any. Initializes TrashConfig with pathPrefix if required.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Entry} entry The entry to find a matching TrashConfig for.
   * @return {?TrashConfig} TrashConfig for entry or null.
   * @private
   */
  getConfig_(volumeManager, entry) {
    const info = volumeManager.getLocationInfo(entry);
    if (!util.isTrashEnabled() || !info) {
      return null;
    }
    const fullPathSlash = entry.fullPath + '/';
    for (const config of TrashConfig.CONFIG) {
      const entryInVolume = fullPathSlash.startsWith(config.topDir);
      if (config.volumeType === info.volumeInfo.volumeType && entryInVolume) {
        if (config.prefixPathWithRemoteMount &&
            info.volumeInfo.remoteMountPath) {
          config.pathPrefix = info.volumeInfo.remoteMountPath;
        }
        return config;
      }
    }
    return null;
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
    const config = this.getConfig_(volumeManager, entry);
    if (config) {
      const fullPathSlash = entry.fullPath + '/';
      const entryInTrash = fullPathSlash.startsWith(config.trashDir);
      if (!entryInTrash) {
        return config;
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
   * @return {!Promise<!TrashEntry|undefined>} Promise which resolves when entry
   *     is removed, rejects with Error.
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

    trashDirs = assert(await TrashDirs.getTrashDirs(
        entry.filesystem, config, /*create=*/ true));

    // Check and remove old items max once per session.
    if (this.inProgress_.has(config.id)) {
      this.removeOldItems_(trashDirs, config, Date.now()).then(() => {
        // In-progress set is not required after removeOldItems_() runs.
        this.inProgress_.delete(config.id);
      });
    }
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
   * @param {!Date} deletionDate deletion date to use in *.trashinfo file.
   * @return {!Promise<!FileEntry>}
   * @private
   */
  async writeTrashInfoFile_(trashInfoDir, trashInfoName, path, deletionDate) {
    return new Promise((resolve, reject) => {
      trashInfoDir.getFile(trashInfoName, {create: true}, infoFile => {
        infoFile.createWriter(writer => {
          writer.onwriteend = () => {
            resolve(infoFile);
          };
          writer.onerror = reject;
          const info = `[Trash Info]\nPath=${path}\nDeletionDate=${
              deletionDate.toISOString()}`;
          writer.write(new Blob([info], {type: 'text/plain'}));
        }, reject);
      }, reject);
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
   * Move a file or a directory to the trash.
   *
   * @param {!Entry} entry The entry to remove.
   * @param {!TrashConfig} config trash config for entry.
   * @return {!Promise<!TrashEntry>}
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
    const inProgress = this.inProgress_.get(config.id);
    if (inProgress) {
      inProgress.add(trashInfoName);
    }
    const path = config.pathPrefix + entry.fullPath;
    const deletionDate = new Date();
    const infoEntry = await this.writeTrashInfoFile_(
        trashDirs.info, trashInfoName, path, deletionDate);
    const filesEntry = await this.moveTo_(entry, trashDirs.files, name);
    return new TrashEntry(
        entry.name, deletionDate, filesEntry, infoEntry, entry);
  }

  /**
   * Restores the specified trash item.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!TrashEntry} trashEntry entry in trash.
   * @return {Promise<void>} Promise which resolves when file is restored.
   */
  async restore(volumeManager, trashEntry) {
    const infoEntry = trashEntry.infoEntry;
    const config = this.getConfig_(volumeManager, infoEntry);
    if (!config) {
      throw new Error(`No TrashConfig for ${infoEntry.toURL()}`);
    }

    // Read Path from info entry.
    const file =
        await new Promise((resolve, reject) => infoEntry.file(resolve, reject));
    const text = await file.text();
    const path = TrashEntry.parsePath(text);
    if (!path) {
      throw new Error(
          `No Path found to restore in ${infoEntry.fullPath}, text=${text}`);
    } else if (!path.startsWith(config.pathPrefix)) {
      throw new Error(`Path does not match expected prefix in ${
          infoEntry.fullPath}, prefix=${config.pathPrefix}, text=${text}`);
    }
    const pathNoLeadingSlash = path.substring(config.pathPrefix.length + 1);
    const parts = pathNoLeadingSlash.split('/');

    // Move to last directory in path, making sure dirs are created if needed.
    let dir = trashEntry.filesEntry.filesystem.root;
    for (let i = 0; i < parts.length - 1; i++) {
      dir =
          assert(await TrashDirs.getDirectory(dir, parts[i], /*create=*/ true));
    }

    // Restore filesEntry first, then remove its trash infoEntry.
    // If any step fails, then either we still have the file in trash with a
    // valid trashinfo, or file is restored and trashinfo will be cleaned up
    // when we remove old items.
    const name =
        await fileOperationUtil.deduplicatePath(dir, parts[parts.length - 1]);
    await this.moveTo_(trashEntry.filesEntry, dir, name);
    // Ignore any error deleting *.trashinfo since DriveFS auto deletes this
    // file when filesEntry is moved.
    await this.permanentlyDeleteFileOrDirectory_(infoEntry).catch(
        e => console.warn(`Error deleting ${infoEntry.toURL()}`, e));
  }

  /**
   * Remove any items from trash older than 30d.
   * @param {!TrashDirs} trashDirs
   * @param {!TrashConfig} config trash config for entry.
   * @param {number} now Current time in milliseconds from epoch.
   */
  async removeOldItems_(trashDirs, config, now) {
    const ls = (reader) => {
      return new Promise((resolve, reject) => {
        reader.readEntries(results => resolve(results), error => reject(error));
      });
    };
    const rm = (entry, log, desc) => {
      if (entry) {
        log(`Deleting ${entry.toURL()}: ${desc}`);
        return this.permanentlyDeleteFileOrDirectory_(entry).catch(
            e => console.warn(`Error deleting ${entry.toURL()}: ${desc}`, e));
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
      console.warn('Error reading old files entries', e);
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
            rm(entry, console.warn, 'Unexpected trash info directory');
            continue;
          }

          // Delete any files not *.trashinfo.
          if (!entry.name.endsWith('.trashinfo')) {
            rm(entry, console.warn, 'Unexpected trash info file');
            continue;
          }

          // Ignore any in-progress files.
          const inProgress = this.inProgress_.get(config.id);
          if (inProgress && inProgress.has(entry.name)) {
            console.log(`Ignoring write in progress ${entry.toURL()}`);
            continue;
          }

          const name = entry.name.substring(0, entry.name.length - 10);
          const filesEntry = filesEntries[name];
          delete filesEntries[name];

          // Delete any .trashinfo file with no matching file entry (unless it
          // was write-in-progress).
          if (!filesEntry) {
            rm(entry, console.warn, 'No matching files entry');
            continue;
          }

          // Delete any entries with no DeletionDate.
          const file = await new Promise(
              (resolve, reject) => entry.file(resolve, reject));
          const text = await file.text();
          const found = text.match(/^DeletionDate=(.*)/m);
          if (!found) {
            rm(entry, console.warn, 'Could not find DeletionDate in ' + text);
            rm(filesEntry, console.warn, 'Invalid matching trashinfo');
            continue;
          }

          // Delete any entries with invalid DeletionDate.
          const d = Date.parse(found[1]);
          if (!d) {
            rm(entry, console.warn, 'Could not parse DeletionDate in ' + text);
            rm(filesEntry, console.warn, 'Invalid matching trashinfo');
            continue;
          }

          // Delete entries older than 30d.
          const ago30d = now - AUTO_DELETE_INTERVAL_MS;
          const ago30dStr = new Date(ago30d).toISOString();
          if (d < ago30d) {
            const msg = `Older than ${ago30dStr}, DeletionDate=${found[1]}`;
            rm(entry, console.log, msg);
            rm(filesEntry, console.log, msg);
          }
        }
      }
    } catch (e) {
      console.warn('Error reading old info entries', e);
      return;
    }

    // Any entries left in filesEntries have no matching *.trashinfo file.
    for (const entry of Object.values(filesEntries)) {
      rm(entry, console.warn, 'No matching *.trashinfo file');
    }
  }
}
