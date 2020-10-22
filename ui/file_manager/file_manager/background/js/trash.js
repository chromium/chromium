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
 * Result from calling Trash.removeFileOrDirectory().
 */
class TrashItem {
  /**
   * @param {string} name
   * @param {!Entry} filesEntry
   * @param {!FileEntry} infoEntry
   */
  constructor(name, filesEntry, infoEntry) {
    this.name = name;
    this.filesEntry = filesEntry;
    this.infoEntry = infoEntry;
  }
}

/**
 * Implementation of trash.
 */
class Trash {
  constructor() {
    /**
     * Store /.Trash/files and /.Trash/info to avoid repeated lookup
     * @private {?TrashDirs}
     */
    this.trashDirs_;
  }

  /**
   * Only move to trash if feature is on, and entry is in MyFiles, but not
   * already in /.Trash.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Entry} entry The entry to remove.
   * @return {boolean} True if item should be moved to trash, else false if item
   *     should be permanently deleted.
   * @private
   */
  shouldMoveToTrash_(volumeManager, entry) {
    if (loadTimeData.getBoolean('FILES_TRASH_ENABLED')) {
      const info = volumeManager.getLocationInfo(entry);
      const entryInTrash =
          entry.fullPath === '/.Trash' || entry.fullPath.startsWith('/.Trash/');
      return !!info &&
          info.rootType === VolumeManagerCommon.RootType.DOWNLOADS &&
          !entryInTrash;
    }
    return false;
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
    if (!permanentlyDelete && this.shouldMoveToTrash_(volumeManager, entry)) {
      return this.trashLocalFileOrDirectory_(volumeManager, entry);
    } else {
      return this.permanentlyDeleteFileOrDirectory_(entry);
    }
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
   * Get /.Trash/files and /.Trash/info directories.
   *
   * @param {!VolumeManager} volumeManager
   * @return {!Promise<!TrashDirs>} Promise which resolves with trash dirs.
   * @private
   */
  getTrashDirs_(volumeManager) {
    if (this.trashDirs_) {
      return Promise.resolve(this.trashDirs_);
    }

    const downloads = volumeManager.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DOWNLOADS);
    const root = downloads.fileSystem.root;
    return new Promise((resolve, reject) => {
      root.getDirectory('.Trash', {create: true}, (trashRoot) => {
        trashRoot.getDirectory('files', {create: true}, (trashFiles) => {
          trashRoot.getDirectory('info', {create: true}, trashInfo => {
            this.trashDirs_ = new TrashDirs(trashFiles, trashInfo);
            resolve(this.trashDirs_);
          }, reject);
        }, reject);
      }, reject);
    });
  }

  /**
   * Write /.Trash/info/<name>.trashinfo file.
   * Creates empty /.Trash/info/<name>.trashinfo.tmp file, writes to file,
   * then moves to /.Trash/info/<name>.trashinfo. By using mv as the final
   * operation we guarantee that another process such as removing old items
   * will not read an incomplete *.trashinfo file.
   *
   * @param {!DirectoryEntry} trashInfoDir /.Trash/info directory.
   * @param {string} name name for <name>.trashinfo file.
   * @param {string} path path to use in .trashinfo file.
   * @return {!Promise<!FileEntry>}
   * @private
   */
  async writeTrashInfoFile_(trashInfoDir, name, path) {
    const tmpName = `${name}.trashinfo.tmp`;
    const finalName = `${name}.trashinfo`;
    const tmpFile = await new Promise((resolve, reject) => {
      trashInfoDir.getFile(tmpName, {create: true}, infoFile => {
        infoFile.createWriter(writer => {
          writer.onwriteend = resolve.bind(null, infoFile);
          writer.onerror = reject;
          const info = `[Trash Info]\nPath=${path}\nDeletionDate=${
              new Date().toISOString()}`;
          writer.write(new Blob([info], {type: 'text/plain'}));
        }, reject);
      }, reject);
    });
    return this.moveTo_(tmpFile, trashInfoDir, finalName);
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
   * Move a file or a directory in the local DOWNLOADS volume to
   * the trash.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Entry} entry The entry to remove.
   * @return {!Promise<!TrashItem>}
   * @private
   */
  async trashLocalFileOrDirectory_(volumeManager, entry) {
    const trashDirs = await this.getTrashDirs_(volumeManager);
    const name =
        await fileOperationUtil.deduplicatePath(trashDirs.files, entry.name);

    // Write trashinfo first, then only move file if info write succeeds.
    // If any step fails, the file will be unchanged, and any partial trashinfo
    // file created will be cleaned up when we remove old items.
    // TODO(crbug.com/953310): Remove old items.
    const infoEntry =
        await this.writeTrashInfoFile_(trashDirs.info, name, entry.fullPath);
    const filesEntry = await this.moveTo_(entry, trashDirs.files, name);
    return new TrashItem(entry.name, filesEntry, infoEntry);
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
    const found = text.match(/^Path=\/(.*)/m);
    if (!found) {
      throw new DOMException(`No Path found to restore in ${
          trashItem.infoEntry.fullPath}, text=${text}`);
    }
    const path = found[1];
    const parts = path.split('/');

    // Move to last directory in path, making sure dirs are created if needed.
    let dir = trashItem.filesEntry.filesystem.root;
    const cd = (directory, path) => {
      return new Promise((resolve, reject) => {
        directory.getDirectory(path, {create: true}, resolve, reject);
      });
    };
    for (let i = 0; i < parts.length - 1; i++) {
      dir = await cd(dir, parts[i]);
    }

    // Restore filesEntry first, then remove its trash infoEntry.
    // If any step fails, then either we still have the file in trash with a
    // valid trashinfo, or file is restored and trashinfo will be cleaned up
    // when we remove old items.
    // TODO(crbug.com/953310): Remove old items.
    const name =
        await fileOperationUtil.deduplicatePath(dir, parts[parts.length - 1]);
    await this.moveTo_(trashItem.filesEntry, dir, name);
    await this.permanentlyDeleteFileOrDirectory_(trashItem.infoEntry);
  }
}
