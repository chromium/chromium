// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {FileType} from '../../common/js/file_type.m.js';
// #import {launcher, LaunchType} from './launcher.m.js';
// #import {util} from '../../common/js/util.m.js';
// #import {volumeManagerFactory} from './volume_manager_factory.m.js';

/**
 * Provides drive search results to chrome launcher.
 */
/* #export */ class LauncherSearch {
  constructor() {
    // Launcher search provider is restricted to dev channel at now.
    if (!chrome.launcherSearchProvider) {
      return;
    }

    /**
     * Active query id. This value is set null when there is no active query.
     * @private {?number}
     */
    this.queryId_ = null;

    /**
     * True if this feature is enabled.
     * @private {boolean}
     */
    this.enabled_ = false;

    /**
     * @private {function(number, string, number)}
     */
    this.onQueryStartedBound_ = this.onQueryStarted_.bind(this);

    /**
     * @private {function(number)}
     */
    this.onQueryEndedBound_ = this.onQueryEnded_.bind(this);

    /**
     * @private {function(string)}
     */
    this.onOpenResultBound_ = this.onOpenResult_.bind(this);

    // This feature is disabled when drive is disabled.
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
  }

  /**
   * Handles onPreferencesChanged event.
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(preferences => {
      this.initializeEventListeners_(
          preferences.driveEnabled, preferences.searchSuggestEnabled);
    });
  }

  /**
   * Initialize event listeners of chrome.launcherSearchProvider.
   *
   * When drive and search suggest are enabled, listen events of
   * chrome.launcherSearchProvider and provide search resutls. When one of them
   * is disabled, remove these event listeners and stop providing search
   * results.
   *
   * @param {boolean} isDriveEnabled True if drive is enabled.
   * @param {boolean} isSearchSuggestEnabled True if search suggest is enabled.
   */
  initializeEventListeners_(isDriveEnabled, isSearchSuggestEnabled) {
    const launcherSearchEnabled = isDriveEnabled && isSearchSuggestEnabled;

    // If this.enabled_ === launcherSearchEnabled, we don't need to change
    // anything here.
    if (this.enabled_ === launcherSearchEnabled) {
      return;
    }

    // Remove event listeners if it's already enabled.
    if (this.enabled_) {
      chrome.launcherSearchProvider.onQueryStarted.removeListener(
          this.onQueryStartedBound_);
      chrome.launcherSearchProvider.onQueryEnded.removeListener(
          this.onQueryEndedBound_);
      chrome.launcherSearchProvider.onOpenResult.removeListener(
          this.onOpenResultBound_);
    }

    // Set queryId_ to null to prevent that on-going search returns search
    // results.
    this.queryId_ = null;

    // Add event listeners when launcher search of Drive is enabled.
    if (launcherSearchEnabled) {
      this.enabled_ = true;
      chrome.launcherSearchProvider.onQueryStarted.addListener(
          this.onQueryStartedBound_);
      chrome.launcherSearchProvider.onQueryEnded.addListener(
          this.onQueryEndedBound_);
      chrome.launcherSearchProvider.onOpenResult.addListener(
          this.onOpenResultBound_);
    } else {
      this.enabled_ = false;
    }
  }

  /**
   * Handles onQueryStarted event.
   * @param {number} queryId
   * @param {string} query
   * @param {number} limit
   */
  onQueryStarted_(queryId, query, limit) {
    this.queryId_ = queryId;

    const startTime = Date.now();
    // Request an instance of volume manager to ensure that all volumes are
    // initialized. When user searches while background page of the Files app is
    // not running, it happens that this method is executed before all volumes
    // are initialized. In this method,
    // chrome.fileManagerPrivate.searchDriveMetadata resolves url internally,
    // and it fails if filesystem of the url is not initialized.
    volumeManagerFactory.getInstance()
        .then(() => {
          return Promise.all([
            this.queryDriveEntries_(queryId, query, limit, startTime),
            this.queryLocalEntries_(query, limit, startTime),
          ]);
        })
        .then((results) => {
          const entries = results[0].concat(results[1]);
          if (queryId !== this.queryId_ || entries.length === 0) {
            return;
          }

          const resultEntries = this.chooseEntries_(entries, query, limit);
          const searchResults = resultEntries.map(this.createSearchResult_);
          chrome.launcherSearchProvider.setSearchResults(
              queryId, searchResults);
        });
  }

  /**
   * Handles onQueryEnded event.
   * @param {number} queryId
   */
  onQueryEnded_(queryId) {
    this.queryId_ = null;
  }

  /**
   * Handles onOpenResult event.
   * @param {string} itemId
   */
  onOpenResult_(itemId) {
    // Request an instance of volume manager to ensure that all volumes are
    // initialized. webkitResolveLocalFileSystemURL in util.urlToEntry fails if
    // filesystem of the url is not initialized.
    volumeManagerFactory.getInstance().then(() => {
      util.urlToEntry(itemId).then(entry => {
        if (entry.isDirectory) {
          // If it's directory, open the directory with file manager.
          launcher.launchFileManager(
              {currentDirectoryURL: entry.toURL()}, undefined, /* App ID */
              LaunchType.FOCUS_SAME_OR_CREATE);
        } else {
          // getFileTasks does not support fake entries.
          if (util.isFakeEntry(entry)) {
            return;
          }
          // If the file is not directory, try to execute default task.
          chrome.fileManagerPrivate.getFileTasks([entry], tasks => {
            // Select default task.
            let defaultTask = null;
            for (let i = 0; i < tasks.length; i++) {
              const task = tasks[i];
              if (task.isDefault) {
                defaultTask = task;
                break;
              }
            }

            // If we haven't picked a default task yet, then just pick the first
            // one which is not generic file handler as default task.
            // TODO(yawano) Share task execution logic with file_tasks.js.
            if (!defaultTask) {
              for (let i = 0; i < tasks.length; i++) {
                const task = tasks[i];
                if (!task.isGenericFileHandler) {
                  defaultTask = task;
                  break;
                }
              }
            }

            if (defaultTask) {
              // Execute default task.
              chrome.fileManagerPrivate.executeTask(
                  defaultTask.taskId, [entry], result => {
                    if (chrome.runtime.lastError) {
                      console.warn(
                          'Unable to execute task: ' +
                          chrome.runtime.lastError.message);
                    }
                    if (result === 'opened' || result === 'message_sent') {
                      return;
                    }
                    this.openFileManagerWithSelectionURL_(entry.toURL());
                  });
            } else {
              // If there is no default task for the url, open a file manager
              // with selecting it.
              // TODO(yawano): Add fallback to view-in-browser as file_tasks.js
              // do
              this.openFileManagerWithSelectionURL_(entry.toURL());
            }
          });
        }
      });
    });
  }

  /**
   * Opens file manager with selecting a specified url.
   * @param {string} selectionURL A url to be selected.
   * @private
   */
  openFileManagerWithSelectionURL_(selectionURL) {
    launcher.launchFileManager(
        {selectionURL: selectionURL}, undefined, /* App ID */
        LaunchType.FOCUS_SAME_OR_CREATE);
  }

  /**
   * Queries entries which match the given query in Google Drive.
   * @param {number} queryId
   * @param {string} query
   * @param {number} limit
   * @param {number} startTime
   * @return {!Promise<!Array<!Entry>>}
   * @private
   */
  queryDriveEntries_(queryId, query, limit, startTime) {
    const param = {
      query: query,
      types: chrome.fileManagerPrivate.SearchType.ALL,
      maxResults: limit
    };
    return new Promise((resolve, reject) => {
      chrome.fileManagerPrivate.searchDriveMetadata(param, results => {
        chrome.fileManagerPrivate.getDriveConnectionState(connectionState => {
          if (connectionState.type !==
              chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE) {
            results = results.filter(
                result => result.entry.isDirectory ||
                    result.availableOffline !== false);
          }
          chrome.metricsPrivate.recordTime(
              'FileBrowser.LauncherSearch.Drive', Date.now() - startTime);
          resolve(results.map(result => result.entry));
        });
      });
    });
  }

  /**
   * Queries entries which match the given query in Downloads.
   * @param {string} query
   * @param {number} startTime
   * @return {!Promise<!Array<!Entry>>}
   * @private
   */
  queryLocalEntries_(query, limit, startTime) {
    if (!query) {
      return Promise.resolve([]);
    }
    return new Promise((resolve, reject) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            query: query,
            types: chrome.fileManagerPrivate.SearchType.ALL,
            maxResults: limit
          },
          results => {
            chrome.metricsPrivate.recordTime(
                'FileBrowser.LauncherSearch.Local', Date.now() - startTime);
            resolve(results);
          });
    });
  }

  /**
   * Chooses entries to show among the given entries.
   * @param {!Array<!Entry>} entries
   * @param {string} query
   * @param {number} limit
   * @return {!Array<!Entry>}
   * @private
   */
  chooseEntries_(entries, query, limit) {
    query = query.toLowerCase();
    const scoreEntry = (entry) => {
      // Prefer entry which has the query string as a prefix.
      if (entry.name.toLowerCase().indexOf(query) === 0) {
        return 1;
      }
      return 0;
    };
    const sortedEntries = entries.sort((a, b) => {
      return scoreEntry(b) - scoreEntry(a);
    });
    return sortedEntries.slice(0, limit);
  }

  /**
   * Creates a search result from entry to pass to launcherSearchProvider API.
   * @param {!Entry} entry
   * @return {!Object}
   * @private
   */
  createSearchResult_(entry) {
    // TODO(yawano): Use filetype_folder_shared.png for a shared
    //     folder.
    let icon = FileType.getIcon(entry);

    if (icon === 'UNKNOWN') {
      icon = 'generic';
    }

    // Hide extensions for hosted files.
    const title = FileType.isHosted(entry) ?
        entry.name.substr(
            0, entry.name.length - FileType.getExtension(entry).length) :
        entry.name;

    return {
      itemId: entry.toURL(),
      title: title,
      iconType: icon,
      // Relevance is set as 2 for all results as a temporary
      // implementation. 2 is the middle value.
      // TODO(yawano): Implement practical relevance calculation.
      relevance: 2
    };
  }
}
