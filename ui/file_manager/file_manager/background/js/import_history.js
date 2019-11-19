// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * @enum {string}
 * @suppress {checkTypes}
 */
importer.ImportHistoryState = {
  'COPIED': 'copied',
  'IMPORTED': 'imported'
};

/**
 * @private @enum {number}
 */
importer.RecordType_ = {
  COPY: 0,
  IMPORT: 1
};

/**
 * @typedef {{
 *   sourceUrl: string,
 *   destinationUrl: string
 * }}
 */
importer.Urls;

/**
 * An {@code ImportHistory} implementation that reads from and
 * writes to a storage object.
 *
 * @implements {importer.ImportHistory}
 *
 */
importer.PersistentImportHistory = class {
  /**
   * @param {function(!FileEntry): !Promise<string>} hashGenerator
   * @param {!importer.RecordStorage} storage
   */
  constructor(hashGenerator, storage) {
    /** @private {function(!FileEntry): !Promise<string>} */
    this.createKey_ = hashGenerator;

    /** @private {!importer.RecordStorage} */
    this.storage_ = storage;

    /**
     * An in-memory representation of local copy history.
     * The first value is the "key" (as generated internally
     * from a file entry).
     * @private {!Object<!Object<!importer.Destination, importer.Urls>>}
     */
    this.copiedEntries_ = {};

    /**
     * An in-memory index from destination URL to key.
     *
     * @private {!Object<string>}
     */
    this.copyKeyIndex_ = {};

    /**
     * An in-memory representation of import history.
     * The first value is the "key" (as generated internally
     * from a file entry).
     * @private {!Object<!Array<importer.Destination>>}
     */
    this.importedEntries_ = {};

    /** @private {!Array<!importer.ImportHistory.Observer>} */
    this.observers_ = [];

    /** @private {Promise<!importer.PersistentImportHistory>} */
    this.whenReady_ = this.load_();
  }

  /**
   * Reloads history from disk. Should be called when the file
   * is changed by an external source.
   *
   * @return {!Promise<!importer.PersistentImportHistory>} Resolves when
   *     history has been refreshed.
   * @private
   */
  load_() {
    return this.storage_.readAll(this.updateInMemoryRecord_.bind(this))
        .then(
            /**
             * @return {!importer.PersistentImportHistory}
             */
            () => {
              return this;
            })
        .catch(importer.getLogger().catcher('import-history-load'));
  }

  /**
   * @return {!Promise<!importer.ImportHistory>}
   */
  whenReady() {
    return /** @type {!Promise<!importer.ImportHistory>} */ (this.whenReady_);
  }

  /**
   * Detects record type and expands records to appropriate arguments.
   *
   * @param {!Array<*>} record
   * @this {importer.PersistentImportHistory}
   */
  updateInMemoryRecord_(record) {
    switch (record[0]) {
      case importer.RecordType_.COPY:
        if (record.length !== 5) {
          importer.getLogger().error(
              'Skipping copy record with wrong number of fields: ' +
              record.length);
          break;
        }
        this.updateInMemoryCopyRecord_(
            /** @type {string} */ (record[1]),  // key
            /** @type {!importer.Destination} */ (record[2]),
            /** @type {string } */ (record[3]),   // sourceUrl
            /** @type {string } */ (record[4]));  // destinationUrl
        return;
      case importer.RecordType_.IMPORT:
        if (record.length !== 3) {
          importer.getLogger().error(
              'Skipping import record with wrong number of fields: ' +
              record.length);
          break;
        }
        this.updateInMemoryImportRecord_(
            /** @type {string } */ (record[1]),  // key
            /** @type {!importer.Destination} */ (record[2]));
        return;
      default:
        assertNotReached(
            'Ignoring record with unrecognized type: ' + record[0]);
    }
  }

  /**
   * Adds an import record to the in-memory history model.
   *
   * @param {string} key
   * @param {!importer.Destination} destination
   *
   * @return {boolean} True if a record was created.
   * @private
   */
  updateInMemoryImportRecord_(key, destination) {
    if (!this.importedEntries_.hasOwnProperty(key)) {
      this.importedEntries_[key] = [destination];
      return true;
    } else if (this.importedEntries_[key].indexOf(destination) === -1) {
      this.importedEntries_[key].push(destination);
      return true;
    }
    return false;
  }

  /**
   * Adds a copy record to the in-memory history model.
   *
   * @param {string} key
   * @param {!importer.Destination} destination
   * @param {string} sourceUrl
   * @param {string} destinationUrl
   *
   * @return {boolean} True if a record was created.
   * @private
   */
  updateInMemoryCopyRecord_(key, destination, sourceUrl, destinationUrl) {
    this.copyKeyIndex_[destinationUrl] = key;
    if (!this.copiedEntries_.hasOwnProperty(key)) {
      this.copiedEntries_[key] = {};
    }
    if (!this.copiedEntries_[key].hasOwnProperty(destination)) {
      this.copiedEntries_[key][destination] = {
        sourceUrl: sourceUrl,
        destinationUrl: destinationUrl
      };
      return true;
    }
    return false;
  }

  /** @override */
  wasCopied(entry, destination) {
    return this.whenReady_.then(this.createKey_.bind(this, entry))
        .then(
            /**
             * @param {string} key
             * @return {boolean}
             */
            key => {
              return key in this.copiedEntries_ &&
                  destination in this.copiedEntries_[key];
            })
        .catch(importer.getLogger().catcher('import-history-was-imported'));
  }

  /** @override */
  wasImported(entry, destination) {
    return this.whenReady_.then(this.createKey_.bind(this, entry))
        .then(
            /**
             * @param {string} key
             * @return {boolean}
             */
            key => {
              return this.getDestinations_(key).indexOf(destination) >= 0;
            })
        .catch(importer.getLogger().catcher('import-history-was-imported'));
  }

  /** @override */
  markCopied(entry, destination, destinationUrl) {
    return this.whenReady_.then(this.createKey_.bind(this, entry))
        .then(
            /**
             * @param {string} key
             * @return {!Promise<?>}
             */
            key => {
              return this.storeRecord_([
                importer.RecordType_.COPY, key, destination,
                importer.deflateAppUrl(entry.toURL()),
                importer.deflateAppUrl(destinationUrl)
              ]);
            })
        .then(this.notifyObservers_.bind(
            this, importer.ImportHistoryState.COPIED, entry, destination,
            destinationUrl))
        .catch(importer.getLogger().catcher('import-history-mark-copied'));
  }

  /** @override */
  listUnimportedUrls(destination) {
    return this.whenReady_
        .then(() => {
          // TODO(smckay): Merge copy and sync records for simpler
          // unimported file discovery.
          const unimported = [];
          for (const key in this.copiedEntries_) {
            const imported = this.importedEntries_[key];
            for (const destination in this.copiedEntries_[key]) {
              if (!imported || imported.indexOf(destination) === -1) {
                const url = importer.inflateAppUrl(
                    this.copiedEntries_[key][destination].destinationUrl);
                unimported.push(url);
              }
            }
          }
          return unimported;
        })
        .catch(importer.getLogger().catcher(
            'import-history-list-unimported-urls'));
  }

  /** @override */
  markImported(entry, destination) {
    return this.whenReady_.then(this.createKey_.bind(this, entry))
        .then(
            /**
             * @param {string} key
             * @return {!Promise<?>}
             */
            key => {
              return this.storeRecord_(
                  [importer.RecordType_.IMPORT, key, destination]);
            })
        .then(this.notifyObservers_.bind(
            this, importer.ImportHistoryState.IMPORTED, entry, destination))
        .catch(importer.getLogger().catcher('import-history-mark-imported'));
  }

  /** @override */
  markImportedByUrl(destinationUrl) {
    const deflatedUrl = importer.deflateAppUrl(destinationUrl);
    const key = this.copyKeyIndex_[deflatedUrl];
    if (key) {
      const copyData = this.copiedEntries_[key];

      // We could build an index of this as well, but it seems
      // unnecessary given the fact that there will almost always
      // be just one destination for a file (assumption).
      for (const destination in copyData) {
        if (copyData[destination].destinationUrl === deflatedUrl) {
          return this
              .storeRecord_([importer.RecordType_.IMPORT, key, destination])
              .then(() => {
                const sourceUrl =
                    importer.inflateAppUrl(copyData[destination].sourceUrl);
                // Here we try to create an Entry for the source URL.
                // This will allow observers to update the UI if the
                // source entry is in view.
                util.urlToEntry(sourceUrl)
                    .then(
                        /**
                         * @param {Entry} entry
                         */
                        entry => {
                          if (entry.isFile) {
                            this.notifyObservers_(
                                importer.ImportHistoryState.IMPORTED,
                                /** @type {!FileEntry} */ (entry), destination);
                          }
                        },
                        () => {
                          console.log(
                              'Unable to find original entry for: ' +
                              sourceUrl);
                          return;
                        })
                    .catch(importer.getLogger().catcher(
                        'notify-listeners-on-import'));
              })
              .catch(importer.getLogger().catcher('mark-imported-by-url'));
        }
      }
    }

    return Promise.reject(
        'Unable to match destination URL to import record > ' + destinationUrl);
  }

  /** @override */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /** @override */
  removeObserver(observer) {
    const index = this.observers_.indexOf(observer);
    if (index > -1) {
      this.observers_.splice(index, 1);
    } else {
      console.warn(
          'Ignoring request to remove observer that is not registered.');
    }
  }

  /**
   * @param {!importer.ImportHistoryState} state
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {string=} opt_destinationUrl
   * @private
   */
  notifyObservers_(state, entry, destination, opt_destinationUrl) {
    this.observers_.forEach(
        /**
         * @param {!importer.ImportHistory.Observer} observer
         * @this {importer.PersistentImportHistory}
         */
        observer => {
          observer({
            state: state,
            entry: entry,
            destination: destination,
            destinationUrl: opt_destinationUrl
          });
        });
  }

  /**
   * @param {!Array<*>} record
   *
   * @return {!Promise<?>} Resolves once the write has been completed.
   * @private
   */
  storeRecord_(record) {
    this.updateInMemoryRecord_(record);
    return this.storage_.write(record);
  }

  /**
   * @param {string} key
   * @return {!Array<string>} The list of previously noted
   *     destinations, or an empty array, if none.
   * @private
   */
  getDestinations_(key) {
    return key in this.importedEntries_ ? this.importedEntries_[key] : [];
  }
};

/**
 * Class responsible for lazy loading of {@code importer.ImportHistory},
 * and reloading when the underlying data is updated (via sync).
 *
 * @implements {importer.HistoryLoader}
 */
importer.SynchronizedHistoryLoader = class {
  /**
   *
   * @param {function(): !Promise<!Array<!FileEntry>>} filesProvider
   */
  constructor(filesProvider) {
    /**
     * @return {!Promise<!Array<!FileEntry>>} History files. Will always
     *     have at least one file (the "primary file"). When other devices
     *     have been used for import, additional files may be present
     *     as well. In all cases the primary file will be used for write
     *     operations and all non-primary files are read-only.
     * @private
     */
    this.getHistoryFiles_ = filesProvider;

    /** @private {boolean} */
    this.needsInitialization_ = true;

    /** @private {!importer.Resolver} */
    this.historyResolver_ = new importer.Resolver();
  }

  /** @override */
  getHistory() {
    if (this.needsInitialization_) {
      this.needsInitialization_ = false;
      this.getHistoryFiles_()
          .then(/**
                 * @param {!Array<!FileEntry>} fileEntries
                 */
                fileEntries => {
                  const storage =
                      new importer.FileBasedRecordStorage(fileEntries);
                  const history = new importer.PersistentImportHistory(
                      importer.createMetadataHashcode, storage);
                  new importer.DriveSyncWatcher(history);
                  history.whenReady().then(() => {
                    this.historyResolver_.resolve(history);
                  });
                })
          .catch(importer.getLogger().catcher('history-load-chain'));
    }

    return this.historyResolver_.promise;
  }

  /** @override */
  addHistoryLoadedListener(listener) {
    this.historyResolver_.promise.then(listener);
  }
};

/**
 * An simple record storage mechanism.
 *
 * @interface
 */
importer.RecordStorage = class {
  /**
   * Adds a new record.
   *
   * @param {!Array<*>} record
   * @return {!Promise<?>} Resolves when record is added.
   */
  write(record) {}

  /**
   * Reads all records.
   *
   * @param {function(!Array<*>)} recordCallback Callback called once
   *     for each record loaded.
   * @return {!Promise<!Array<*>>}
   */
  readAll(recordCallback) {}
};

/**
 * A {@code RecordStore} that persists data in a {@code FileEntry}.
 * @implements {importer.RecordStorage}
 */
importer.FileBasedRecordStorage = class {
  /**
   * @param {!Array<!FileEntry>} fileEntries The first entry is the
   *     "primary" file for read-write, all other are read-only
   *     sources of data (presumably synced from other machines).
   */
  constructor(fileEntries) {
    /** @private {!Array<!importer.PromisingFileEntry>} */
    this.inputFiles_ = fileEntries.map(importer.PromisingFileEntry.create);

    /** @private {!importer.PromisingFileEntry} */
    this.outputFile_ = this.inputFiles_[0];

    /**
     * Serializes all writes and reads on the primary file.
     * @private {!Promise<?>}
     * */
    this.latestOperation_ = Promise.resolve(null);
  }

  /** @override */
  write(record) {
    return this.latestOperation_ =
               this.latestOperation_
                   .then(
                       /**
                        * @param {?} ignore
                        */
                       ignore => {
                         return this.outputFile_.createWriter();
                       })
                   .then(this.writeRecord_.bind(this, record))
                   .catch(
                       importer.getLogger().catcher('file-record-store-write'));
  }

  /**
   * Appends a new record to the end of the file.
   *
   * @param {!Object} record
   * @param {!FileWriter} writer
   * @return {!Promise<?>} Resolves when write is complete.
   * @private
   */
  writeRecord_(record, writer) {
    const blob = new Blob(
        [JSON.stringify(record) + ',\n'], {type: 'text/plain; charset=UTF-8'});

    return new Promise(
        /**
         * @param {function()} resolve
         * @param {function()} reject
         * @this {importer.FileBasedRecordStorage}
         */
        (resolve, reject) => {
          writer.onwriteend = resolve;
          writer.onerror = reject;

          writer.seek(writer.length);
          writer.write(blob);
        });
  }

  /** @override */
  readAll(recordCallback) {
    return this.latestOperation_ =
               this.latestOperation_
                   .then(
                       /**
                        * @param {?} ignored
                        */
                       ignored => {
                         const filePromises = this.inputFiles_.map(
                             /**
                              * @param {!importer.PromisingFileEntry} entry
                              * @this {importer.FileBasedRecordStorage}
                              */
                             entry => {
                               return entry.file();
                             });
                         return Promise.all(filePromises);
                       })
                   .then(
                       /**
                        * @return {!Promise<!Array<string>>}
                        */
                       files => {
                         const contentPromises =
                             files.map(this.readFileAsText_.bind(this));
                         return Promise.all(contentPromises);
                       },
                       /**
                        * @return {string}
                        */
                       () => {
                         console.error(
                             'Unable to read from one of history files.');
                         return '';
                       })
                   .then(
                       /**
                        * @param {!Array<string>} fileContents
                        */
                       fileContents => {
                         const parsePromises =
                             fileContents.map(this.parse_.bind(this));
                         return Promise.all(parsePromises);
                       })
                   .then(
                       /** @param {!Array<!Array<*>>} parsedContents */
                       parsedContents => {
                         parsedContents.forEach(
                             /** @param {!Array<!Array<*>>} recordSet */
                             recordSet => {
                               recordSet.forEach(recordCallback);
                             });
                       })
                   .catch(importer.getLogger().catcher(
                       'file-record-store-read-all'));
  }

  /**
   * Reads the entire entry as a single string value.
   *
   * @param {!File} file
   * @return {!Promise<string>}
   * @private
   */
  readFileAsText_(file) {
    return new Promise((resolve, reject) => {
             const reader = new FileReader();

             reader.onloadend = () => {
               if (reader.error) {
                 console.error(reader.error);
                 reject();
               } else {
                 resolve(reader.result);
               }
             };

             reader.onerror = error => {
               console.error(error);
               reject(error);
             };

             reader.readAsText(file);
           })
        .catch(importer.getLogger().catcher(
            'file-record-store-read-file-as-text'));
  }

  /**
   * Parses the text.
   *
   * @param {string} text
   * @return {!Array<!Array<*>>}
   * @private
   */
  parse_(text) {
    if (text.length === 0) {
      return [];
    } else {
      // Dress up the contents of the file like an array,
      // so the JSON object can parse it using JSON.parse.
      // That means we need to both:
      //   1) Strip the trailing ',\n' from the last record
      //   2) Surround the whole string in brackets.
      // NOTE: JSON.parse is WAY faster than parsing this
      // ourselves in javascript.
      const json = '[' + text.substring(0, text.length - 2) + ']';
      return /** @type {!Array<!Array<*>>} */ (JSON.parse(json));
    }
  }
};

/**
 * This class makes the "drive" badges appear by way of marking entries as
 * imported in history when a previously imported file is fully synced to drive.
 */
importer.DriveSyncWatcher = class {
  /**
   * @param {!importer.ImportHistory} history
   */
  constructor(history) {
    /** @private {!importer.ImportHistory} */
    this.history_ = history;

    this.history_.addObserver(this.onHistoryChanged_.bind(this));

    this.history_.whenReady()
        .then(() => {
          this.history_.listUnimportedUrls(importer.Destination.GOOGLE_DRIVE)
              .then(this.updateSyncStatus_.bind(
                  this, importer.Destination.GOOGLE_DRIVE));
        })
        .catch(importer.getLogger().catcher('drive-sync-watcher-constructor'));

    // Listener is only registered once the history object is initialized.
    // No need to register synchonously since we don't want to be
    // woken up to respond to events.
    chrome.fileManagerPrivate.onFileTransfersUpdated.addListener(
        this.onFileTransfersUpdated_.bind(this));
    // TODO(smckay): Listen also for errors on onDriveSyncError.
  }

  /**
   * @param {!importer.Destination} destination
   * @param {!Array<string>} unimportedUrls
   * @private
   */
  updateSyncStatus_(destination, unimportedUrls) {
    // TODO(smckay): Chunk processing of urls...to ensure we're not
    // blocking interactive tasks. For now, we just defer the update
    // for a few seconds.
    setTimeout(() => {
      unimportedUrls.forEach(url => {
        this.checkSyncStatus_(destination, url);
      });
    }, importer.DriveSyncWatcher.UPDATE_DELAY_MS);
  }

  /**
   * @param {!chrome.fileManagerPrivate.FileTransferStatus} status
   * @private
   */
  onFileTransfersUpdated_(status) {
    // If the synced file it isn't one we copied,
    // the call to mark by url will just fail...fine by us.
    if (status.transferState === 'completed') {
      this.history_.markImportedByUrl(status.fileUrl);
    }
  }

  /**
   * @param {!importer.ImportHistory.ChangedEvent} event
   * @private
   */
  onHistoryChanged_(event) {
    if (event.state === importer.ImportHistoryState.COPIED) {
      // Check sync status in case the file synced *before* it was able
      // to mark be marked as copied.
      this.checkSyncStatus_(
          event.destination,
          /**@type {string}*/ (event.destinationUrl), event.entry);
    }
  }

  /**
   * @param {!importer.Destination} destination
   * @param {string} url
   * @param {!FileEntry=} opt_entry Pass this if you have an entry
   *     on hand, else, we'll jump through some extra hoops to
   *     make do without it.
   * @private
   */
  checkSyncStatus_(destination, url, opt_entry) {
    console.assert(
        destination === importer.Destination.GOOGLE_DRIVE,
        'Unsupported destination: ' + destination);

    this.getSyncStatus_(url)
        .then(
            /**
             * @param {boolean} synced True if file is synced
             */
            synced => {
              if (synced) {
                if (opt_entry) {
                  this.history_.markImported(opt_entry, destination);
                } else {
                  this.history_.markImportedByUrl(url);
                }
              }
            })
        .catch(importer.getLogger().catcher(
            'drive-sync-watcher-check-sync-status'));
  }

  /**
   * @param {string} url
   * @return {!Promise<boolean>} Resolves with true if the
   *     file has been synced to the named destination.
   * @private
   */
  getSyncStatus_(url) {
    return util.URLsToEntries([url])
        .then(function(results) {
          if (results.entries.length !== 1) {
            return Promise.reject();
          }
          return new Promise(
              /** @this {importer.DriveSyncWatcher} */
              (resolve, reject) => {
                // TODO(smckay): User Metadata Cache...once it is available
                // in the background.
                chrome.fileManagerPrivate.getEntryProperties(
                    [results.entries[0]], ['dirty'],
                    /**
                     * @param
                     * {!Array<!chrome.fileManagerPrivate.EntryProperties>|undefined}
                     * propertiesList
                     * @this {importer.DriveSyncWatcher}
                     */
                    propertiesList => {
                      console.assert(
                          propertiesList.length === 1,
                          'Got an unexpected number of results.');
                      if (chrome.runtime.lastError) {
                        reject(chrome.runtime.lastError);
                      } else {
                        const data = propertiesList[0];
                        resolve(!data['dirty']);
                      }
                    });
              });
        })
        .catch(
            importer.getLogger().catcher('drive-sync-watcher-get-sync-status'));
  }
};

/** @const {number} */
importer.DriveSyncWatcher.UPDATE_DELAY_MS = 3500;

/**
 * @param {!FileEntry} fileEntry
 * @return {!Promise<string>} Resolves with a "hashcode" consisting of
 *     just the last modified time and the file size.
 */
importer.createMetadataHashcode = function(fileEntry) {
  return new Promise((resolve, reject) => {
           metadataProxy.getEntryMetadata(fileEntry).then(
               /**
                * @param {!Object} metadata
                */
               metadata => {
                 if (!('modificationTime' in metadata)) {
                   reject('File entry missing "modificationTime" field.');
                 } else if (!('size' in metadata)) {
                   reject('File entry missing "size" field.');
                 } else {
                   const secondsSinceEpoch =
                       importer.toSecondsFromEpoch(metadata.modificationTime);
                   resolve(secondsSinceEpoch + '_' + metadata.size);
                 }
               });
         })
      .catch(importer.getLogger().catcher('importer-common-create-hashcode'));
};
