// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Trash UI entry types based on
 * https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html.
 *
 * When you move /dir/hello.txt to trash, you get:
 *  .Trash/files/hello.txt
 *  .Trash/info/hello.trashinfo
 *
 * .Trash/files/hello.txt is the original file.  .Trash/files.hello.trashinfo is
 * a text file which looks like:
 *  [Trash Info]
 *  Path=/dir/hello.txt
 *  DeletionDate=2020-11-02T07:35:38.964Z
 *
 * TrashEntry combines both files for display.
 */

/**
 * Used to control the display of items in Trash. Combines the info from both
 * .Trash/info and ./Trash/files.
 *
 * @implements {FilesAppEntry}
 */
class TrashEntry {
  /**
   * @param {string} path Path of deleted file from infoEntry.
   * @param {!Date} deletionDate DeletionDate of deleted file from infoEntry.
   * @param {!FileEntry} infoEntry trash info entry.
   * @param {!Entry} filesEntry trash files entry.
   * @param {string} rootLabel Root label to prefix display name.
   */
  constructor(path, deletionDate, infoEntry, filesEntry, rootLabel) {
    /** @private */
    this.deletionDate_ = deletionDate;

    /** @private */
    this.infoEntry_ = infoEntry;

    /** @private */
    this.filesEntry_ = filesEntry;

    /** @override Entry */
    this.filesystem = filesEntry.filesystem;

    /** @override Entry */
    this.fullPath = path;

    /** @override Entry */
    this.isDirectory = filesEntry.isDirectory;

    /** @override Entry  */
    this.isFile = filesEntry.isFile;

    /**
     * Show the root label and the whole Path=<path> from infoEntry as the name.
     * This allows users to differentiate deleted files such as /a/hello.txt and
     * /b/hello.txt.
     * @override Entry
     */
    this.name = rootLabel + path.replace(/\//g, ' › ');

    /** @override FileEntry */
    this.file = filesEntry.file;

    /** @override FilesAppEntry */
    this.rootType = VolumeManagerCommon.RootType.TRASH;

    /** @override FilesAppEntry */
    this.type_name = 'TrashEntry';
  }

  /** @override Entry */
  toURL() {
    return 'trash://' + this.infoEntry_.toURL();
  }

  /**
   * Pass through to getMetadata() of filesEntry, keep size, but use
   * DeletionDate from infoEntry for modificationTime.
   *
   * @override Entry
   */
  getMetadata(success, error) {
    this.filesEntry_.getMetadata(m => {
      success({modificationTime: this.deletionDate_, size: m.size});
    }, error);
  }

  /** @override Entry */
  getParent() {
    return null;
  }

  /** @override FilesAppEntry */
  get isNativeType() {
    return false;
  }

  /** @override FilesAppEntry */
  getNativeEntry() {
    return null;
  }
}
/**
 * Reads all entries in each of .Trash/info and .Trash/files and produces a
 * single stream of TrashEntry.
 *
 * @extends {DirectoryReader}
 */
class TrashDirectoryReader {
  /**
   * @param {!FileSystem} fileSystem trash file system.
   * @param {!TrashConfig} config trash config.
   * @param {string} rootLabel Label of trash root to prefix entries with.
   */
  constructor(fileSystem, config, rootLabel) {
    this.fileSystem_ = fileSystem;
    this.config_ = config;
    this.rootLabel_ = rootLabel;

    /** @private {!Object<!Entry>} all entries in .Trash/files keyed by name. */
    this.filesEntries_ = {};

    /**
     * DirectoryReader of .Trash/info which needs to be persisted across calls
     * to readEntries().
     *
     * @private {?DirectoryReader}
     */
    this.infoReader_ = null;
  }

  /**
   * Create a trash entry if infoEntry and matching files entry are valid, else
   * return null.
   *
   * @param {!FileEntry} infoEntry trash info entry.
   * @return {!Promise<TrashEntry?>}
   */
  async createTrashEntry_(infoEntry) {
    const error = (msg, text = '') => {
      console.error(`${msg}: ${infoEntry.toURL()}: ${text}`);
      return null;
    };

    // Ignore any directories.
    if (!infoEntry.isFile) {
      return error('Ignoring unexpected trash info directory');
    }

    // Ignore any files not *.trashinfo.
    if (!infoEntry.name.endsWith('.trashinfo')) {
      return error('Ignoring unexpected trash info file');
    }

    const name = infoEntry.name.substring(0, infoEntry.name.length - 10);
    const filesEntry = this.filesEntries_[name];
    delete this.filesEntries_[name];

    // Ignore any .trashinfo file with no matching file entry.
    if (!filesEntry) {
      return error('Ignoring trash info file with no matching files entry');
    }

    const file =
        await new Promise((resolve, reject) => infoEntry.file(resolve, reject));
    const text = await file.text();
    let found = text.match(/^Path=(.*)/m);
    if (!found) {
      return error('Ignoring trash info file with no Path', text);
    }
    const path = found[1];

    found = text.match(/^DeletionDate=(.*)/m);
    if (!found) {
      return error('Ignoring trash info file with no DeletionDate', text);
    }

    const d = Date.parse(found[1]);
    if (!found) {
      return error('Ignoring trash info file with invalid DeletionDate', text);
    }

    return new TrashEntry(
        path, new Date(d), infoEntry, filesEntry, this.rootLabel_);
  }

  /**
   * Async version of readEntries(). This function may be called multiple times
   * and returns an empty result to indicate end of stream.
   *
   * Reads all items in .Trash/files on first call and caches them. Then reads
   * 1 or more batches of infoReader until we have at least 1 valid result to
   * send, or reader is exhausted.
   *
   * @param {function(!Array<!Entry>)} success
   * @param {function(!FileError)=} error
   */
  async readEntriesAsync_(success, error) {
    const ls = (reader) => {
      return new Promise((resolve, reject) => {
        reader.readEntries(results => resolve(results), error => reject(error));
      });
    };

    // Read all of .Trash/files on first call.
    if (!this.infoReader_) {
      const trashDirs =
          await TrashDirs.getTrashDirs(this.fileSystem_, this.config_);

      // Get all entries in trash/files.
      const filesReader = trashDirs.files.createReader();
      try {
        while (true) {
          const entries = await ls(filesReader);
          if (!entries.length) {
            break;
          }
          entries.forEach(entry => this.filesEntries_[entry.name] = entry);
        }
      } catch (e) {
        console.error('Error reading trash files entries', e);
        error(e);
        return;
      }

      this.infoReader_ = trashDirs.info.createReader();
    }

    // Consume infoReader which is initialized in the first call. Read from
    // .Trash/info until we have at least 1 result, or end of stream.
    const result = [];
    while (true) {
      let entries = [];
      try {
        entries = await ls(this.infoReader_);
      } catch (e) {
        console.error('Error reading trash info entries', e);
        error(e);
        return;
      }
      for (const e of entries) {
        const trashEntry = await this.createTrashEntry_(e);
        if (trashEntry) {
          result.push(trashEntry);
        }
      }
      if (!entries.length || result.length) {
        break;
      }
    }
    success(result);
  }

  /** @override */
  readEntries(success, error) {
    this.readEntriesAsync_(success, error);
  }
}

/**
 * Root Trash entry sits inside "My files". It shows the combined entries of
 * trashes defined in TrashConfig.
 */
class TrashRootEntry extends FakeEntryImpl {
  /**
   * @param {!VolumeManager} volumeManager
   */
  constructor(volumeManager) {
    super('Trash', VolumeManagerCommon.RootType.TRASH);
    this.volumeManager_ = volumeManager;
  }

  /** @override */
  createReader() {
    const readers = [];
    TrashConfig.CONFIG.forEach(c => {
      const info =
          this.volumeManager_.getCurrentProfileVolumeInfo(c.volumeType);
      if (info && info.fileSystem) {
        const locationInfo =
            this.volumeManager_.getLocationInfo(info.fileSystem.root);
        const rootLabel = util.getRootTypeLabel(assert(locationInfo));
        readers.push(new TrashDirectoryReader(info.fileSystem, c, rootLabel));
      }
    });
    return new CombinedReaders(readers);
  }
}
