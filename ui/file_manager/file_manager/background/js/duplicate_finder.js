// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {importer} from '../../common/js/importer_common.js';
import {LRUCache} from '../../common/js/lru_cache.js';
import {metrics} from '../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {duplicateFinderInterfaces} from '../../externs/background/duplicate_finder.js';
import {importerHistoryInterfaces} from '../../externs/background/import_history.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {importerHistory} from './import_history.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

const duplicateFinder = {};

/**
 * A duplicate finder for Google Drive.
 *
 */
duplicateFinder.DriveDuplicateFinder = class {
  constructor() {
    /** @private {Promise<string>} */
    this.driveIdPromise_ = null;

    /**
     * An bounded cache of most recently calculated file content hashcodes.
     * @private {!LRUCache<!Promise<string>>}
     */
    this.hashCache_ = new LRUCache(
        duplicateFinder.DriveDuplicateFinder.MAX_CACHED_HASHCODES_);
  }

  /**
   * @param {!FileEntry} entry
   * @return {!Promise<boolean>}
   */
  isDuplicate(entry) {
    return this.computeHash_(entry)
        .then(this.findByHash_.bind(this))
        .then(
            /**
             * @param {!Array<string>} urls
             * @return {boolean}
             */
            urls => {
              return urls.length > 0;
            });
  }

  /**
   * Computes the content hash for the given file entry.
   * @param {!FileEntry} entry
   * @return {!Promise<string>} The computed hash.
   * @private
   */
  computeHash_(entry) {
    return importerHistory.createMetadataHashcode(entry).then(hashcode => {
      // Cache key is the concatenation of metadata hashcode and URL.
      const cacheKey = hashcode + '|' + entry.toURL();
      if (this.hashCache_.hasKey(cacheKey)) {
        return this.hashCache_.get(cacheKey);
      }

      const hashPromise = new Promise(
          /** @this {duplicateFinder.DriveDuplicateFinder} */
          (resolve, reject) => {
            const startTime = new Date().getTime();
            chrome.fileManagerPrivate.computeChecksum(
                entry,
                /**
                 * @param {string|undefined} result The content hash.
                 * @this {duplicateFinder.DriveDuplicateFinder}
                 */
                result => {
                  const elapsedTime = new Date().getTime() - startTime;
                  // Send the timing to GA only if it is sorta exceptionally
                  // long. A one second, CPU intensive operation, is pretty
                  // long.
                  if (elapsedTime >= duplicateFinder.DriveDuplicateFinder
                                         .HASH_EVENT_THRESHOLD_) {
                    console.info(
                        'Content hash computation took ' + elapsedTime +
                        ' ms.');
                    metrics.recordTime(
                        'DriveDuplicateFinder.LongComputeHash', elapsedTime);
                  }
                  if (chrome.runtime.lastError) {
                    reject(chrome.runtime.lastError);
                  } else {
                    resolve(result);
                  }
                });
          });

      this.hashCache_.put(cacheKey, hashPromise);
      return hashPromise;
    });
  }

  /**
   * Finds files with content hashes matching the given hash.
   * @param {string} hash The content hash of the file to find.
   * @return {!Promise<!Array<string>>} The URLs of the found files.  If there
   *     are no matches, the list will be empty.
   * @private
   */
  findByHash_(hash) {
    return /** @type {!Promise<!Array<string>>} */ (
        this.getDriveId_().then(this.searchFilesByHash_.bind(this, hash)));
  }

  /**
   * @return {!Promise<string>} ID of the user's Drive volume.
   * @private
   */
  getDriveId_() {
    if (!this.driveIdPromise_) {
      this.driveIdPromise_ = volumeManagerFactory.getInstance().then(
          /**
           * @param {!VolumeManager} volumeManager
           * @return {string} ID of the user's Drive volume.
           */
          volumeManager => {
            return volumeManager
                .getCurrentProfileVolumeInfo(
                    VolumeManagerCommon.VolumeType.DRIVE)
                .volumeId;
          });
    }
    return this.driveIdPromise_;
  }

  /**
   * A promise-based wrapper for chrome.fileManagerPrivate.searchFilesByHashes.
   * @param {string} hash The content hash to search for.
   * @param {string} volumeId The volume to search.
   * @return {!Promise<!Array<string>>} A list of file URLs.
   * @private
   */
  searchFilesByHash_(hash, volumeId) {
    return new Promise(
        /** @this {duplicateFinder.DriveDuplicateFinder} */
        (resolve, reject) => {
          const startTime = new Date().getTime();
          chrome.fileManagerPrivate.searchFilesByHashes(
              volumeId, [hash],
              /**
               * @param {!Object<string, !Array<string>>|undefined} urls
               */
              urls => {
                const elapsedTime = new Date().getTime() - startTime;
                // Send the timing to GA only if it is sorta exceptionally long.
                if (elapsedTime >= duplicateFinder.DriveDuplicateFinder
                                       .SEARCH_EVENT_THRESHOLD_) {
                  metrics.recordTime(
                      'DriveDuplicateFinder.LongSearchByHash', elapsedTime);
                }
                if (chrome.runtime.lastError) {
                  reject(chrome.runtime.lastError);
                } else {
                  resolve(urls[hash]);
                }
              });
        });
  }
};

/** @private @const {number} */
duplicateFinder.DriveDuplicateFinder.HASH_EVENT_THRESHOLD_ = 5000;

/** @private @const {number} */
duplicateFinder.DriveDuplicateFinder.SEARCH_EVENT_THRESHOLD_ = 1000;

/** @private @const {number} */
duplicateFinder.DriveDuplicateFinder.MAX_CACHED_HASHCODES_ = 10000;


/**
 * A class that aggregates history/content-dupe checking
 * into a single "Disposition" value. Should now be the
 * primary source for duplicate checking (with the exception
 * of in-scan deduplication, where duplicate results that
 * are within the scan are ignored).
 * @implements {duplicateFinderInterfaces.DispositionChecker}
 */
duplicateFinder.DispositionCheckerImpl = class {
  /**
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   * @param {!duplicateFinder.DriveDuplicateFinder} contentMatcher
   */
  constructor(historyLoader, contentMatcher) {
    /** @private {!importerHistoryInterfaces.HistoryLoader} */
    this.historyLoader_ = historyLoader;

    /** @private {!duplicateFinder.DriveDuplicateFinder} */
    this.contentMatcher_ = contentMatcher;
  }

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {!importer.ScanMode} mode
   * @return {!Promise<!importer.Disposition>}
   */
  getDisposition(entry, destination, mode) {
    if (destination !== importer.Destination.GOOGLE_DRIVE) {
      return Promise.reject('Unsupported destination: ' + destination);
    }

    if (window.isSWA) {
      return Promise.reject('Cloud Import is disabled in the SWA');
    }

    return new Promise(
        /** @this {duplicateFinderInterfaces.DispositionChecker} */
        (resolve, reject) => {
          this.hasHistoryDuplicate_(entry, destination)
              .then(
                  /**
                   * @param {boolean} duplicate
                   */
                  duplicate => {
                    if (duplicate) {
                      resolve(importer.Disposition.HISTORY_DUPLICATE);
                      return;
                    }
                    if (mode == importer.ScanMode.HISTORY) {
                      resolve(importer.Disposition.ORIGINAL);
                      return;
                    }
                    this.contentMatcher_.isDuplicate(entry).then(
                        /** @param {boolean} duplicate */
                        duplicate => {
                          if (duplicate) {
                            resolve(importer.Disposition.CONTENT_DUPLICATE);
                          } else {
                            resolve(importer.Disposition.ORIGINAL);
                          }
                        });
                  });
        });
  }

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} True if there is a history-entry-duplicate
   *     for the file.
   * @private
   */
  hasHistoryDuplicate_(entry, destination) {
    return this.historyLoader_.getHistory().then(
        /**
         * @param {!importerHistoryInterfaces.ImportHistory} history
         * @return {!Promise}
         */
        history => {
          return Promise
              .all([
                history.wasCopied(entry, destination),
                history.wasImported(entry, destination),
              ])
              .then(
                  /**
                   * @param {!Array<boolean>} results
                   * @return {boolean}
                   */
                  results => {
                    return results[0] || results[1];
                  });
        });
  }

  /**
   * Factory for a function that returns an entry's disposition.
   *
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   *
   * @return {!duplicateFinderInterfaces.DispositionChecker.CheckerFunction}
   */
  static createChecker(historyLoader) {
    const checker = new duplicateFinder.DispositionCheckerImpl(
        historyLoader, new duplicateFinder.DriveDuplicateFinder());
    return checker.getDisposition.bind(checker);
  }
};

export {duplicateFinder};
