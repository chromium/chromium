// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * @typedef {{file:File, externalFileUrl:string}}
 */
var FileAsyncData;

/**
 * @param {!Document} doc Owning document.
 * @param {!DirectoryTree} directoryTree Directory tree.
 * @param {!ListContainer} listContainer List container.
 * @param {!MultiProfileShareDialog} multiProfileShareDialog Share dialog to be
 *     used to share files from another profile.
 * @param {function(boolean, !Array<string>): !Promise<boolean>}
 *     confirmationCallback called when operation requires user's confirmation.
 *     The opeartion will be executed if the return value resolved to true.
 * @param {!ProgressCenter} progressCenter To notify starting copy operation.
 * @param {!FileOperationManager} fileOperationManager File operation manager
 *     instance.
 * @param {!MetadataModel} metadataModel Metadata cache service.
 * @param {!ThumbnailModel} thumbnailModel
 * @param {!DirectoryModel} directoryModel Directory model instance.
 * @param {!VolumeManager} volumeManager Volume manager instance.
 * @param {!FileSelectionHandler} selectionHandler Selection handler.
 * @param {function((!Entry|!FakeEntry)): boolean} shouldShowCommandFor
 * @struct
 * @constructor
 */
function FileTransferController(
    doc, listContainer, directoryTree, multiProfileShareDialog,
    confirmationCallback, progressCenter, fileOperationManager, metadataModel,
    thumbnailModel, directoryModel, volumeManager, selectionHandler,
    shouldShowCommandFor) {
  /**
   * @private {!Document}
   * @const
   */
  this.document_ = doc;

  /**
   * @private {!ListContainer}
   * @const
   */
  this.listContainer_ = listContainer;

  /**
   * @private {!FileOperationManager}
   * @const
   */
  this.fileOperationManager_ = fileOperationManager;

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {!ThumbnailModel}
   * @const
   */
  this.thumbnailModel_ = thumbnailModel;

  /**
   * @private {!DirectoryModel}
   * @const
   */
  this.directoryModel_ = directoryModel;

  /**
   * @private {!VolumeManager}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!FileSelectionHandler}
   * @const
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @private {!MultiProfileShareDialog}
   * @const
   */
  this.multiProfileShareDialog_ = multiProfileShareDialog;

  /**
   * @private {function(boolean, !Array<string>):
   *     Promise<boolean>}
   * @const
   */
  this.confirmationCallback_ = confirmationCallback;

  /**
   * @private {!ProgressCenter}
   * @const
   */
  this.progressCenter_ = progressCenter;

  /**
   * @private {function((!Entry|!FakeEntry)): boolean}
   * @const
   */
  this.shouldShowCommandFor_ = shouldShowCommandFor;

  /**
   * The array of pending task ID.
   * @type {Array<string>}
   */
  this.pendingTaskIds = [];

  /**
   * Promise to be fulfilled with the thumbnail image of selected file in drag
   * operation. Used if only one element is selected.
   * @private {Promise}
   */
  this.preloadedThumbnailImagePromise_ = null;

  /**
   * File objects for selected files.
   *
   * @private {Object<FileAsyncData>}
   */
  this.selectedAsyncData_ = {};

  /**
   * Drag selector.
   * @private {DragSelector}
   */
  this.dragSelector_ = new DragSelector();

  /**
   * Whether a user is touching the device or not.
   * @private {boolean}
   */
  this.touching_ = false;

  /**
   * Count of the SourceNotFound error.
   * @private {number}
   */
  this.sourceNotFoundErrorCount_ = 0;

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.copyCommand_ = /** @type {!cr.ui.Command} */ (
      queryRequiredElement('command#copy', this.document_));

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.cutCommand_ = /** @type {!cr.ui.Command} */ (
      queryRequiredElement('command#cut', this.document_));

  /**
   * @private {DirectoryEntry|FilesAppDirEntry}
   */
  this.destinationEntry_ = null;

  /**
   * @private {EventTarget}
   */
  this.lastEnteredTarget_ = null;

  /**
   * @private {Element}
   */
  this.dropTarget_ = null;

  /**
   * The element for showing a label while dragging files.
   * @private {Element}
   */
  this.dropLabel_ = null;

  /**
   * @private {number}
   */
  this.navigateTimer_ = 0;

  // Register the events.
  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onFileSelectionChanged_.bind(this));
  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE_THROTTLED,
      this.onFileSelectionChangedThrottled_.bind(this));
  this.attachDragSource_(listContainer.table.list);
  this.attachFileListDropTarget_(listContainer.table.list);
  this.attachDragSource_(listContainer.grid);
  this.attachFileListDropTarget_(listContainer.grid);
  this.attachTreeDropTarget_(directoryTree);
  this.attachCopyPasteHandlers_();

  // Allow to drag external files to the browser window.
  chrome.fileManagerPrivate.enableExternalFileScheme();
}

/**
 * Size of drag thumbnail for image files.
 *
 * @type {number}
 * @const
 * @private
 */
FileTransferController.DRAG_THUMBNAIL_SIZE_ = 64;

/**
 * Y coordinate of the label to describe drop action, relative to mouse cursor.
 *
 * @type {number}
 * @const
 * @private
 */
FileTransferController.DRAG_LABEL_Y_OFFSET_ = -32;

/**
 * Container for defining a copy/move operation.
 *
 * @param {!Array<string>} sourceURLs URLs of source entries.
 * @param {!DirectoryEntry} destinationEntry Destination directory.
 * @param {!EntryLocation} destinationLocationInfo Location info of the
 *     destination directory.
 * @param {boolean} isMove true if move, false if copy.
 * @constructor
 * @struct
 */
FileTransferController.PastePlan = function(
    sourceURLs, destinationEntry, destinationLocationInfo, isMove) {
  /**
   * @type {!Array<string>}
   * @const
   */
  this.sourceURLs = sourceURLs;

  /**
   * @type {!DirectoryEntry}
   */
  this.destinationEntry = destinationEntry;

  /**
   * @type {!EntryLocation}
   */
  this.destinationLocationInfo = destinationLocationInfo;

  /**
   * @type {boolean}
   * @const
   */
  this.isMove = isMove;
};

/**
 * Confirmation message types.
 *
 * @enum {string}
 */
FileTransferController.ConfirmationType = {
  NONE: 'none',
  MOVE_BETWEEN_TEAM_DRIVES: 'between_team_drives',
  MOVE_FROM_TEAM_DRIVE_TO_OTHER: 'move_from_team_drive_to_other',
  MOVE_FROM_OTHER_TO_TEAM_DRIVE: 'move_from_other_to_team_drive',
  COPY_FROM_OTHER_TO_TEAM_DRIVE: 'copy_from_other_to_team_drive',
};

/**
 * Obtains whether the planned operation requires user's confirmation, as well
 * as its type.
 *
 * @param {!Array<!Entry>} sourceEntries
 * @return {FileTransferController.ConfirmationType} type of the confirmation
 *     required for the operation. If no confirmation is needed,
 *     FileTransferController.ConfirmationType.NONE will be returned.
 */
FileTransferController.PastePlan.prototype.getConfirmationType = function(
    sourceEntries) {
  assert(sourceEntries.length != 0);
  var source = {
    isTeamDrive: util.isTeamDriveEntry(sourceEntries[0]),
    teamDriveName: util.getTeamDriveName(sourceEntries[0])
  };
  var destination = {
    isTeamDrive: util.isTeamDriveEntry(this.destinationEntry),
    teamDriveName: util.getTeamDriveName(this.destinationEntry)
  };
  if (this.isMove) {
    if (source.isTeamDrive) {
      if (destination.isTeamDrive) {
        if (source.teamDriveName == destination.teamDriveName)
          return FileTransferController.ConfirmationType.NONE;
        else
          return FileTransferController.ConfirmationType
              .MOVE_BETWEEN_TEAM_DRIVES;
      } else {
        return FileTransferController.ConfirmationType
            .MOVE_FROM_TEAM_DRIVE_TO_OTHER;
      }
    } else if (destination.isTeamDrive) {
      return FileTransferController.ConfirmationType
          .MOVE_FROM_OTHER_TO_TEAM_DRIVE;
    }
    return FileTransferController.ConfirmationType.NONE;
  } else {
    if (!destination.isTeamDrive)
      return FileTransferController.ConfirmationType.NONE;
    // Copying to Team Drive.
    if (!(source.isTeamDrive &&
          source.teamDriveName == destination.teamDriveName)) {
      // This is not a copy within the same Team Drive.
      return FileTransferController.ConfirmationType
          .COPY_FROM_OTHER_TO_TEAM_DRIVE;
    }
    return FileTransferController.ConfirmationType.NONE;
  }
};

/**
 * Composes a confirmation message for the given type.
 *
 * @param {FileTransferController.ConfirmationType} confirmationType
 * @return {!Array<string>} sentences for a confirmation dialog box.
 */
FileTransferController.PastePlan.prototype.getConfirmationMessages = function(
    confirmationType, sourceEntries) {
  assert(sourceEntries.length != 0);
  var sourceName = util.getTeamDriveName(sourceEntries[0]);
  var destinationName = util.getTeamDriveName(this.destinationEntry);
  switch (confirmationType) {
    case FileTransferController.ConfirmationType.MOVE_BETWEEN_TEAM_DRIVES:
      return [
        strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName),
        strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName)
      ];
    // TODO(yamaguchi): notify ownership transfer if the two Team Drives
    // belong to different domains.
    case FileTransferController.ConfirmationType.MOVE_FROM_TEAM_DRIVE_TO_OTHER:
      return [
        strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName)
        // TODO(yamaguchi): Warn if the operation moves at least one
        // directory to My Drive, as it's no undoable.
      ];
    case FileTransferController.ConfirmationType.MOVE_FROM_OTHER_TO_TEAM_DRIVE:
      return [strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS', destinationName)];
    case FileTransferController.ConfirmationType.COPY_FROM_OTHER_TO_TEAM_DRIVE:
      return [strf(
          'DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName)];
  }
  assertNotReached('Invalid confirmation type: ' + confirmationType);
  return [];
};

/**
 * Converts list of urls to list of Entries with granting R/W permissions to
 * them, which is essential when pasting files from a different profile.
 *
 * @param {!Array<string>} urls Urls to be converted.
 * @return {Promise<!Array<string>>}
 */
FileTransferController.URLsToEntriesWithAccess = function(urls) {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.grantAccess(urls, resolve.bind(null, undefined));
  }).then(function() {
    return util.URLsToEntries(urls);
  });
};

/**
 * @param {!cr.ui.List} list Items in the list will be draggable.
 * @private
 */
FileTransferController.prototype.attachDragSource_ = function(list) {
  list.style.webkitUserDrag = 'element';
  list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
  list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
  list.addEventListener('touchstart', this.onTouchStart_.bind(this));
  list.ownerDocument.addEventListener(
      'touchend', this.onTouchEnd_.bind(this), true);
  list.ownerDocument.addEventListener(
      'touchcancel', this.onTouchEnd_.bind(this), true);
};

/**
 * @param {!cr.ui.List} list List itself and its directory items will could
 *                          be drop target.
 * @param {boolean=} opt_onlyIntoDirectories If true only directory list
 *     items could be drop targets. Otherwise any other place of the list
 *     accetps files (putting it into the current directory).
 * @private
 */
FileTransferController.prototype.attachFileListDropTarget_ =
    function(list, opt_onlyIntoDirectories) {
  list.addEventListener('dragover', this.onDragOver_.bind(this,
      !!opt_onlyIntoDirectories, list));
  list.addEventListener('dragenter',
      this.onDragEnterFileList_.bind(this, list));
  list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
  list.addEventListener('drop',
      this.onDrop_.bind(this, !!opt_onlyIntoDirectories));
};

/**
 * @param {!DirectoryTree} tree Its sub items will could be drop target.
 * @private
 */
FileTransferController.prototype.attachTreeDropTarget_ = function(tree) {
  tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
  tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
  tree.addEventListener('dragleave', this.onDragLeave_.bind(this, tree));
  tree.addEventListener('drop', this.onDrop_.bind(this, true));
};

/**
 * Attach handlers of copy, cut and paste operations to the document.
 * @private
 */
FileTransferController.prototype.attachCopyPasteHandlers_ = function() {
  this.document_.addEventListener('beforecopy',
                                  this.onBeforeCutOrCopy_.bind(
                                      this, false /* not move operation */));
  this.document_.addEventListener('copy',
                                  this.onCutOrCopy_.bind(
                                      this, false /* not move operation */));
  this.document_.addEventListener('beforecut',
                                  this.onBeforeCutOrCopy_.bind(
                                      this, true /* move operation */));
  this.document_.addEventListener('cut',
                                  this.onCutOrCopy_.bind(
                                      this, true /* move operation */));
  this.document_.addEventListener('beforepaste',
                                  this.onBeforePaste_.bind(this));
  this.document_.addEventListener('paste',
                                  this.onPaste_.bind(this));
};

/**
 * Write the current selection to system clipboard.
 *
 * @param {!ClipboardData} clipboardData ClipboardData from the event.
 * @param {string} effectAllowed Value must be valid for the
 *     |clipboardData.effectAllowed| property.
 * @private
 */
FileTransferController.prototype.cutOrCopy_ = function(
    clipboardData, effectAllowed) {
  var currentDirEntry = this.directoryModel_.getCurrentDirEntry();
  if (!currentDirEntry)
    return;
  var volumeInfo = this.volumeManager_.getVolumeInfo(
      util.isRecentRoot(currentDirEntry) ?
          this.selectionHandler_.selection.entries[0] :
          currentDirEntry);
  if (!volumeInfo)
    return;

  this.appendCutOrCopyInfo_(clipboardData, effectAllowed, volumeInfo,
      this.selectionHandler_.selection.entries,
      !this.selectionHandler_.isAvailable());
  this.appendUriList_(clipboardData,
      this.selectionHandler_.selection.entries);
};

/**
 * Appends copy or cut information of |entries| to |clipboardData|.
 * @param {!ClipboardData} clipboardData ClipboardData from the event.
 * @param {string} effectAllowed Value must be valid for the
 *     |clipboardData.effectAllowed| property.
 * @param {!VolumeInfo} sourceVolumeInfo
 * @param {!Array<!Entry>} entries
 * @param {boolean} missingFileContents
 * @private
 */
FileTransferController.prototype.appendCutOrCopyInfo_ = function(
    clipboardData, effectAllowed, sourceVolumeInfo, entries,
    missingFileContents) {
  // Tag to check it's filemanager data.
  clipboardData.setData('fs/tag', 'filemanager-data');
  clipboardData.setData('fs/sourceRootURL',
                       sourceVolumeInfo.fileSystem.root.toURL());

  var sourceURLs = util.entriesToURLs(entries);
  clipboardData.setData('fs/sources', sourceURLs.join('\n'));

  clipboardData.effectAllowed = effectAllowed;
  clipboardData.setData('fs/effectallowed', effectAllowed);

  clipboardData.setData('fs/missingFileContents',
      missingFileContents.toString());
};

/**
 * Appends uri-list of |entries| to |clipboardData|.
 * @param {!ClipboardData} clipboardData ClipboardData from the event.
 * @param {!Array<!Entry>} entries
 * @private
 */
FileTransferController.prototype.appendUriList_ = function(
    clipboardData, entries) {
  var externalFileUrl;

  for (var i = 0; i < entries.length; i++) {
    var url = entries[i].toURL();
    if (!this.selectedAsyncData_[url])
      continue;
    if (this.selectedAsyncData_[url].file)
      clipboardData.items.add(this.selectedAsyncData_[url].file);
    if (!externalFileUrl)
      externalFileUrl = this.selectedAsyncData_[url].externalFileUrl;
  }

  if (externalFileUrl)
    clipboardData.setData('text/uri-list', externalFileUrl);
};

/**
 * @return {Object<string>} Drag and drop global data object.
 * @private
 */
FileTransferController.prototype.getDragAndDropGlobalData_ = function() {
  if (window[DRAG_AND_DROP_GLOBAL_DATA])
    return window[DRAG_AND_DROP_GLOBAL_DATA];

  // Dragging from other tabs/windows.
  var views = chrome && chrome.extension ? chrome.extension.getViews() : [];
  for (var i = 0; i < views.length; i++) {
    if (views[i][DRAG_AND_DROP_GLOBAL_DATA])
      return views[i][DRAG_AND_DROP_GLOBAL_DATA];
  }
  return null;
};

/**
 * Extracts source root URL from the |clipboardData| or |dragAndDropData|
 * object.
 *
 * @param {!ClipboardData} clipboardData DataTransfer object from the event.
 * @param {Object<string>} dragAndDropData The drag and drop data from
 *     getDragAndDropGlobalData_().
 * @return {string} URL or an empty string (if unknown).
 * @private
 */
FileTransferController.prototype.getSourceRootURL_ = function(
    clipboardData, dragAndDropData) {
  var sourceRootURL = clipboardData.getData('fs/sourceRootURL');
  if (sourceRootURL)
    return sourceRootURL;

  // |clipboardData| in protected mode.
  if (dragAndDropData)
    return dragAndDropData.sourceRootURL;

  // Unknown source.
  return '';
};

/**
 * @param {!ClipboardData} clipboardData DataTransfer object from the event.
 * @return {boolean} Returns true when missing some file contents.
 * @private
 */
FileTransferController.prototype.isMissingFileContents_ =
    function(clipboardData) {
  var data = clipboardData.getData('fs/missingFileContents');
  if (!data) {
    // |clipboardData| in protected mode.
    var globalData = this.getDragAndDropGlobalData_();
    if (globalData)
      data = globalData.missingFileContents;
  }
  return data === 'true';
};

/**
 * Obtains entries that need to share with me.
 * The method also observers child entries of the given entries.
 * @param {Array<Entry>} entries Entries.
 * @return {!Promise<Array<Entry>>} Promise to be fulfilled with the entries
 *    that need to share.
 * @private
 */
FileTransferController.prototype.getMultiProfileShareEntries_ =
    function(entries) {
  // Utility function to concat arrays.
  var concatArrays = function(arrays) {
    return Array.prototype.concat.apply([], arrays);
  };

  // Call processEntry for each item of entries.
  var processEntries = function(entries) {
    var files = entries.filter(function(entry) {return entry.isFile;});
    var dirs = entries.filter(function(entry) {return !entry.isFile;});
    var promises = dirs.map(processDirectoryEntry);
    if (files.length > 0)
      promises.push(processFileEntries(files));
    return Promise.all(promises).then(concatArrays);
  };

  // Check all file entries and keeps only those need sharing operation.
  var processFileEntries = function(entries) {
    return new Promise(function(callback) {
      // Do not use metadata cache here because the urls come from the different
      // profile.
      chrome.fileManagerPrivate.getEntryProperties(
          entries, ['hosted', 'sharedWithMe'], callback);
    }).then(function(metadatas) {
      return entries.filter(function(entry, i) {
        var metadata = metadatas[i];
        return metadata && metadata.hosted && !metadata.sharedWithMe;
      });
    });
  };

  // Check child entries.
  var processDirectoryEntry = function(entry) {
    return readEntries(entry.createReader());
  };

  // Read entries from DirectoryReader and call processEntries for the chunk
  // of entries.
  var readEntries = function(reader) {
    return new Promise(reader.readEntries.bind(reader)).then(
        function(entries) {
          if (entries.length > 0) {
            return Promise.all(
                [processEntries(entries), readEntries(reader)]).
                then(concatArrays);
          } else {
            return [];
          }
        },
        function(error) {
          console.warn(
              'Error happens while reading directory.', error);
          return [];
        });
  }.bind(this);

  // Filter entries that is owned by the current user, and call
  // processEntries.
  return processEntries(entries.filter(function(entry) {
    // If the volumeInfo is found, the entry belongs to the current user.
    return !this.volumeManager_.getVolumeInfo(entry);
  }.bind(this)));
};

/**
 * Collects parameters of paste operation by the given command and the current
 * system clipboard.
 *
 * @return {!FileTransferController.PastePlan}
 */
FileTransferController.prototype.preparePaste = function(
    clipboardData, opt_destinationEntry, opt_effect) {
  var sourceURLs = clipboardData.getData('fs/sources') ?
      clipboardData.getData('fs/sources').split('\n') : [];
  // effectAllowed set in copy/paste handlers stay uninitialized. DnD handlers
  // work fine.
  var effectAllowed = clipboardData.effectAllowed !== 'uninitialized' ?
      clipboardData.effectAllowed : clipboardData.getData('fs/effectallowed');
  var destinationEntry = opt_destinationEntry ||
      /** @type {DirectoryEntry} */ (this.directoryModel_.getCurrentDirEntry());
  var toMove = util.isDropEffectAllowed(effectAllowed, 'move') &&
      (!util.isDropEffectAllowed(effectAllowed, 'copy') ||
       opt_effect === 'move');

  var destinationLocationInfo =
      this.volumeManager_.getLocationInfo(destinationEntry);
  if (!destinationLocationInfo)
    console.log(
        'Failed to get destination location for ' + destinationEntry.title() +
        ' while attempting to paste files.');
  return new FileTransferController.PastePlan(
      sourceURLs, destinationEntry, assert(destinationLocationInfo), toMove);
};

/**
 * Queue up a file copy operation based on the current system clipboard and
 * drag-and-drop global object.
 *
 * @param {!ClipboardData} clipboardData System data transfer object.
 * @param {DirectoryEntry=} opt_destinationEntry Paste destination.
 * @param {string=} opt_effect Desired drop/paste effect. Could be
 *     'move'|'copy' (default is copy). Ignored if conflicts with
 *     |clipboardData.effectAllowed|.
 * @return {!Promise<string>} Either "copy" or "move".
 */
FileTransferController.prototype.paste = function(
    clipboardData, opt_destinationEntry, opt_effect) {
  var pastePlan =
      this.preparePaste(clipboardData, opt_destinationEntry, opt_effect);

  return util.URLsToEntries(pastePlan.sourceURLs).then(function(entriesResult) {
    const sourceEntries = entriesResult.entries;
    const destinationEntry = pastePlan.destinationEntry;
    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);

    const destinationIsOutsideOfDrive =
        VolumeManagerCommon.getVolumeTypeFromRootType(
            destinationLocationInfo.rootType) !==
        VolumeManagerCommon.VolumeType.DRIVE;

    // Disallow transferring hosted files from Team Drives to outside of Drive.
    // This is because hosted files aren't 'real' files, so it doesn't make
    // sense to allow a 'local' copy (e.g. in Downloads, or on a USB), where the
    // file can't be accessed offline (or necessarily accessed at all) by the
    // person who tries to open it.
    // In future, block this for all hosted files, regardless of their source.
    // For now, to maintain backwards-compatibility, just block this for hosted
    // files stored in a Team Drive.
    if (sourceEntries.some(
            entry =>
                util.isTeamDriveEntry(entry) && FileType.isHosted(entry)) &&
        destinationIsOutsideOfDrive) {
      // For now, just don't execute the paste.
      // TODO(sashab): Display a warning message, and disallow drag-drop
      // operations.
      return null;
    }

    if (sourceEntries.length == 0) {
      // This can happen when copied files were deleted before pasting them.
      // We execute the plan as-is, so as to share the post-copy logic.
      // This is basically same as getting empty by filtering same-directory
      // entries.
      return Promise.resolve(this.executePaste(pastePlan));
    }
    var confirmationType = pastePlan.getConfirmationType(sourceEntries);
    if (confirmationType == FileTransferController.ConfirmationType.NONE) {
      return Promise.resolve(this.executePaste(pastePlan));
    }
    var messages =
        pastePlan.getConfirmationMessages(confirmationType, sourceEntries);
    this.confirmationCallback_(pastePlan.isMove, messages)
        .then(function(userApproved) {
          if (userApproved) {
            this.executePaste(pastePlan);
          }
        }.bind(this));
  }.bind(this));
};

/**
 * Queue up a file copy operation.
 *
 * @param {FileTransferController.PastePlan} pastePlan
 * @return {string} Either "copy" or "move".
 */
FileTransferController.prototype.executePaste = function(pastePlan) {
  var sourceURLs = pastePlan.sourceURLs;
  var toMove = pastePlan.isMove;
  var destinationEntry = pastePlan.destinationEntry;

  var entries = [];
  var failureUrls;
  var shareEntries;
  var taskId = this.fileOperationManager_.generateTaskId();

  // Creates early progress center item for faster user feedback.
  var item = new ProgressCenterItem();
  item.id = taskId;
  if (toMove) {
    item.message = strf('MOVE_ITEMS_REMAINING', sourceURLs.length);
  } else {
    item.message = strf('COPY_ITEMS_REMAINING', sourceURLs.length);
  }
  this.progressCenter_.updateItem(item);

  FileTransferController.URLsToEntriesWithAccess(sourceURLs)
      .then(
          (/**
           * @param {Object} result
           * @this {FileTransferController}
           */
          function(result) {
            failureUrls = result.failureUrls;
            // The promise is not rejected, so it's safe to not remove the early
            // progress center item here.
            return this.fileOperationManager_.filterSameDirectoryEntry(
                result.entries, destinationEntry, toMove);
          }).bind(this))
      .then(
          (/**
           * @param {!Array<Entry>} filteredEntries
           * @this {FileTransferController}
           * @return {!Promise<Array<Entry>>}
           */
          function(filteredEntries) {
            entries = filteredEntries;
            if (entries.length === 0) {
              // Remove the early progress center item.
              var item = new ProgressCenterItem();
              item.id = taskId;
              item.state = ProgressItemState.CANCELED;
              this.progressCenter_.updateItem(item);
              return Promise.reject('ABORT');
            }

            this.pendingTaskIds.push(taskId);
            var item = new ProgressCenterItem();
            item.id = taskId;
            if (toMove) {
              item.type = ProgressItemType.MOVE;
              if (entries.length === 1)
                item.message = strf('MOVE_FILE_NAME', entries[0].name);
              else
                item.message = strf('MOVE_ITEMS_REMAINING', entries.length);
            } else {
              item.type = ProgressItemType.COPY;
              if (entries.length === 1)
                item.message = strf('COPY_FILE_NAME', entries[0].name);
              else
                item.message = strf('COPY_ITEMS_REMAINING', entries.length);
            }
            this.progressCenter_.updateItem(item);
            // Check if cross share is needed or not.
            return this.getMultiProfileShareEntries_(entries);
          }).bind(this))
      .then(
          (/**
           * @param {Array<Entry>} inShareEntries
           * @this {FileTransferController}
           * @return {!Promise<Array<Entry>>|!Promise<null>}
           */
          function(inShareEntries) {
            shareEntries = inShareEntries;
            if (shareEntries.length === 0)
              return Promise.resolve(null);
            return this.multiProfileShareDialog_.
                showMultiProfileShareDialog(shareEntries.length > 1);
          }).bind(this))
      .then(
          /**
           * @param {?string} dialogResult
           * @return {!Promise<undefined>|undefined}
           */
           function(dialogResult) {
              if (dialogResult === null)
                return;  // No dialog was shown, skip this step.
              if (dialogResult === 'cancel')
                return Promise.reject('ABORT');
              // Do cross share.
              // TODO(hirono): Make the loop cancellable.
              var requestDriveShare = function(index) {
                if (index >= shareEntries.length)
                  return;
                return new Promise(function(fulfill) {
                  chrome.fileManagerPrivate.requestDriveShare(
                      shareEntries[index],
                      assert(dialogResult),
                      function() {
                        // TODO(hirono): Check chrome.runtime.lastError here.
                        fulfill();
                      });
                }).then(requestDriveShare.bind(null, index + 1));
              };
              return requestDriveShare(0);
            })
      .then(
          (/** @this {FileTransferController} */
          function() {
            // Start the pasting operation.
            this.fileOperationManager_.paste(
                entries, destinationEntry, toMove, taskId);
            this.pendingTaskIds.splice(this.pendingTaskIds.indexOf(taskId), 1);

            // Publish source not found error item.
            for (var i = 0; i < failureUrls.length; i++) {
              var fileName =
                  decodeURIComponent(failureUrls[i].replace(/^.+\//, ''));
              var item = new ProgressCenterItem();
              item.id = 'source-not-found-' + this.sourceNotFoundErrorCount_;
              if (toMove)
                item.message = strf('MOVE_SOURCE_NOT_FOUND_ERROR', fileName);
              else
                item.message = strf('COPY_SOURCE_NOT_FOUND_ERROR', fileName);
              item.state = ProgressItemState.ERROR;
              this.progressCenter_.updateItem(item);
              this.sourceNotFoundErrorCount_++;
            }
          }).bind(this))
      .catch(
          function(error) {
            if (error !== 'ABORT')
              console.error(error.stack ? error.stack : error);
          });
  return toMove ? 'move' : 'copy';
};

/**
 * Preloads an image thumbnail for the specified file entry.
 *
 * @param {Entry} entry Entry to preload a thumbnail for.
 * @private
 */
FileTransferController.prototype.preloadThumbnailImage_ = function(entry) {
  var imagePromise = this.thumbnailModel_.get([entry]).then(function(metadata) {
    return new Promise(function(fulfill, reject) {
      var loader = new ThumbnailLoader(
          entry, ThumbnailLoader.LoaderType.IMAGE, metadata[0]);
      loader.loadDetachedImage(function(result) {
        if (result)
          fulfill(loader.getImage());
      });
    });
  });

  imagePromise.then(function(image) {
    // Store the image so that we can obtain the image synchronously.
    imagePromise.value = image;
  });

  this.preloadedThumbnailImagePromise_ = imagePromise;
};

/**
 * Renders a drag-and-drop thumbnail.
 *
 * @return {!Element} Element containing the thumbnail.
 * @private
 */
FileTransferController.prototype.renderThumbnail_ = function() {
  var length = this.selectionHandler_.selection.entries.length;
  var container = this.document_.querySelector('#drag-container');
  var contents = this.document_.createElement('div');
  contents.className = 'drag-contents';
  container.appendChild(contents);

  // Option 1. Multiple selection, render only a label.
  if (length > 1) {
    var label = this.document_.createElement('div');
    label.className = 'label';
    label.textContent = strf('DRAGGING_MULTIPLE_ITEMS', length);
    contents.appendChild(label);
    return container;
  }

  // Option 2. Thumbnail image available from preloadedThumbnailImagePromise_,
  // then render it without a label.
  if (this.preloadedThumbnailImagePromise_ &&
      this.preloadedThumbnailImagePromise_.value) {
    var thumbnailImage = this.preloadedThumbnailImagePromise_.value;

    // Resize the image to canvas.
    var canvas = document.createElement('canvas');
    canvas.width = FileTransferController.DRAG_THUMBNAIL_SIZE_;
    canvas.height = FileTransferController.DRAG_THUMBNAIL_SIZE_;

    var minScale = Math.min(
        thumbnailImage.width / canvas.width,
        thumbnailImage.height / canvas.height);
    var srcWidth = Math.min(canvas.width * minScale, thumbnailImage.width);
    var srcHeight = Math.min(canvas.height * minScale, thumbnailImage.height);

    var context = canvas.getContext('2d');
    context.drawImage(thumbnailImage,
                      (thumbnailImage.width - srcWidth) / 2,
                      (thumbnailImage.height - srcHeight) / 2,
                      srcWidth,
                      srcHeight,
                      0,
                      0,
                      canvas.width,
                      canvas.height);
    contents.classList.add('for-image');
    contents.appendChild(canvas);
    return container;
  }

  // Option 3. Thumbnail image available from file grid / list, render it
  // without a label.
  // Because of Option 1, there is only exactly one item selected.
  var index = this.selectionHandler_.selection.indexes[0];
  // We only need one of the thumbnails.
  var thumbnail = this.listContainer_.currentView.getThumbnail(index);
  if (thumbnail) {
    var canvas = document.createElement('canvas');
    canvas.width = FileTransferController.DRAG_THUMBNAIL_SIZE_;
    canvas.height = FileTransferController.DRAG_THUMBNAIL_SIZE_;
    canvas.style.backgroundImage = thumbnail.style.backgroundImage;
    canvas.style.backgroundSize = 'cover';
    canvas.classList.add('for-image');
    contents.appendChild(canvas);
    return container;
  }

  // Option 4. Thumbnail not available. Render an icon and a label.
  var entry = this.selectionHandler_.selection.entries[0];
  var icon = this.document_.createElement('div');
  icon.className = 'detail-icon';
  icon.setAttribute('file-type-icon', FileType.getIcon(entry));
  contents.appendChild(icon);
  var label = this.document_.createElement('div');
  label.className = 'label';
  label.textContent = entry.name;
  contents.appendChild(label);
  return container;
};

/**
 * @param {!cr.ui.List} list Drop target list
 * @param {!Event} event A dragstart event of DOM.
 * @private
 */
FileTransferController.prototype.onDragStart_ = function(list, event) {
  // If renaming is in progress, drag operation should be used for selecting
  // substring of the text. So we don't drag files here.
  if (this.listContainer_.renameInput.currentEntry) {
    event.preventDefault();
    return;
  }

  // If this drag operation is initiated by mouse, check if we should start
  // selecting area.
  if (!this.touching_ && list.shouldStartDragSelection(event)) {
    event.preventDefault();
    this.dragSelector_.startDragSelection(list, event);
    return;
  }

  // If the drag starts outside the files list on a touch device, cancel the
  // drag.
  if (this.touching_ && !list.hasDragHitElement(event)) {
    event.preventDefault();
    list.selectionModel_.unselectAll();
    return;
  }

  // Nothing selected.
  if (!this.selectionHandler_.selection.entries.length) {
    event.preventDefault();
    return;
  }

  var dt = event.dataTransfer;
  var canCopy = this.canCopyOrDrag_();
  var canCut = this.canCutOrDrag_();
  if (canCopy || canCut) {
    if (canCopy && canCut) {
      this.cutOrCopy_(dt, 'all');
    } else if (canCopy) {
      this.cutOrCopy_(dt, 'copyLink');
    } else {
      this.cutOrCopy_(dt, 'move');
    }
  } else {
    event.preventDefault();
    return;
  }

  var dragThumbnail = this.renderThumbnail_();
  dt.setDragImage(dragThumbnail, 0, 0);

  window[DRAG_AND_DROP_GLOBAL_DATA] = {
    sourceRootURL: dt.getData('fs/sourceRootURL'),
    missingFileContents: dt.getData('fs/missingFileContents')
  };
};

/**
 * @param {!cr.ui.List} list Drop target list.
 * @param {!Event} event A dragend event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnd_ = function(list, event) {
  // TODO(fukino): This is workaround for crbug.com/373125.
  // This should be removed after the bug is fixed.
  this.touching_ = false;

  var container = this.document_.querySelector('#drag-container');
  container.textContent = '';
  this.clearDropTarget_();
  delete window[DRAG_AND_DROP_GLOBAL_DATA];
};

/**
 * @param {boolean} onlyIntoDirectories True if the drag is only into
 *     directories.
 * @param {(!cr.ui.List|!DirectoryTree)} list Drop target list.
 * @param {Event} event A dragover event of DOM.
 * @private
 */
FileTransferController.prototype.onDragOver_ =
    function(onlyIntoDirectories, list, event) {
  event.preventDefault();
  var entry = this.destinationEntry_;
  if (!entry && !onlyIntoDirectories)
    entry = this.directoryModel_.getCurrentDirEntry();
  var effectAndLabel =
      this.selectDropEffect_(event, this.getDragAndDropGlobalData_(), entry);
  event.dataTransfer.dropEffect = effectAndLabel.getDropEffect();
  event.preventDefault();
  var label = effectAndLabel.getLabel();
  if (!this.dropLabel_) {
    this.dropLabel_ = document.querySelector("div#drop-label");
  }
  if (label) {
    this.dropLabel_.innerText = label;
    this.dropLabel_.style.left = event.pageX + 'px';
    this.dropLabel_.style.top =
        (event.pageY + FileTransferController.DRAG_LABEL_Y_OFFSET_) + 'px';
    this.dropLabel_.style.display = 'block';
  } else {
    this.dropLabel_.style.display = 'none';
  }
};

/**
 * @param {(!cr.ui.List|!DirectoryTree)} list Drop target list.
 * @param {!Event} event A dragenter event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnterFileList_ = function(list, event) {
  event.preventDefault();  // Required to prevent the cursor flicker.
  this.lastEnteredTarget_ = event.target;
  var item = list.getListItemAncestor(
      /** @type {HTMLElement} */ (event.target));
  item = item && list.isItem(item) ? item : null;
  if (item === this.dropTarget_)
    return;

  var entry = item && list.dataModel.item(item.listIndex);
  if (entry)
    this.setDropTarget_(item, event.dataTransfer, entry);
  else
    this.clearDropTarget_();
};

/**
 * @param {!DirectoryTree} tree Drop target tree.
 * @param {!Event} event A dragenter event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnterTree_ = function(tree, event) {
  event.preventDefault();  // Required to prevent the cursor flicker.
  this.lastEnteredTarget_ = event.target;
  var item = event.target;
  while (item && !(item instanceof cr.ui.TreeItem)) {
    item = item.parentNode;
  }

  if (item === this.dropTarget_)
    return;

  var entry = item && item.entry;
  if (entry) {
    this.setDropTarget_(item, event.dataTransfer, entry);
  } else {
    this.clearDropTarget_();
  }
};

/**
 * @param {*} list Drop target list.
 * @param {Event} event A dragleave event of DOM.
 * @private
 */
FileTransferController.prototype.onDragLeave_ = function(list, event) {
  // If mouse moves from one element to another the 'dragenter'
  // event for the new element comes before the 'dragleave' event for
  // the old one. In this case event.target !== this.lastEnteredTarget_
  // and handler of the 'dragenter' event has already caried of
  // drop target. So event.target === this.lastEnteredTarget_
  // could only be if mouse goes out of listened element.
  if (event.target === this.lastEnteredTarget_) {
    this.clearDropTarget_();
    this.lastEnteredTarget_ = null;
  }
  if (this.dropLabel_) {
    this.dropLabel_.style.display = 'none';
  }
};

/**
 * @param {boolean} onlyIntoDirectories True if the drag is only into
 *     directories.
 * @param {!Event} event A dragleave event of DOM.
 * @private
 */
FileTransferController.prototype.onDrop_ =
    function(onlyIntoDirectories, event) {
  if (onlyIntoDirectories && !this.dropTarget_)
    return;
  var destinationEntry = this.destinationEntry_ ||
                         this.directoryModel_.getCurrentDirEntry();
  if (!this.canPasteOrDrop_(event.dataTransfer, destinationEntry))
    return;
  event.preventDefault();
  this.paste(
      event.dataTransfer,
      /** @type {DirectoryEntry} */ (destinationEntry),
      this.selectDropEffect_(
              event, this.getDragAndDropGlobalData_(), destinationEntry)
          .getDropEffect());
  this.clearDropTarget_();
};

/**
 * Change to the drop target directory.
 * @private
 */
FileTransferController.prototype.changeToDropTargetDirectory_ = function() {
  // Do custom action.
  if (this.dropTarget_ instanceof DirectoryItem)
    /** @type {DirectoryItem} */ (this.dropTarget_).doDropTargetAction();
  this.directoryModel_.changeDirectoryEntry(assert(this.destinationEntry_));
};

/**
 * Sets the drop target.
 *
 * @param {Element} domElement Target of the drop.
 * @param {!ClipboardData} clipboardData Data transfer object.
 * @param {!DirectoryEntry|!FakeEntry} destinationEntry Destination entry.
 * @private
 */
FileTransferController.prototype.setDropTarget_ =
    function(domElement, clipboardData, destinationEntry) {
  if (this.dropTarget_ === domElement)
    return;

  // Remove the old drop target.
  this.clearDropTarget_();

  // Set the new drop target.
  this.dropTarget_ = domElement;

  if (!domElement || !destinationEntry.isDirectory) {
    return;
  }

  // Add accept class if the domElement can accept the drag.
  domElement.classList.add('accepts');
  this.destinationEntry_ = destinationEntry;

  // Change directory immediately for crostini, otherwise start timer.
  if (destinationEntry.rootType === VolumeManagerCommon.RootType.CROSTINI) {
    this.changeToDropTargetDirectory_();
  } else {
    this.navigateTimer_ =
        setTimeout(this.changeToDropTargetDirectory_.bind(this), 2000);
  }
};

/**
 * Handles touch start.
 * @private
 */
FileTransferController.prototype.onTouchStart_ = function() {
  this.touching_ = true;
};

/**
 * Handles touch end.
 * @private
 */
FileTransferController.prototype.onTouchEnd_ = function() {
  // TODO(fukino): We have to check if event.touches.length be 0 to support
  // multi-touch operations, but event.touches has incorrect value by a bug
  // (crbug.com/373125).
  // After the bug is fixed, we should check event.touches.
  this.touching_ = false;
};

/**
 * Clears the drop target.
 * @private
 */
FileTransferController.prototype.clearDropTarget_ = function() {
  if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts'))
    this.dropTarget_.classList.remove('accepts');
  this.dropTarget_ = null;
  this.destinationEntry_ = null;
  if (this.navigateTimer_ !== undefined) {
    clearTimeout(this.navigateTimer_);
    this.navigateTimer_ = 0;
  }
};

/**
 * @return {boolean} Returns false if {@code <input type="text">} element is
 *     currently active. Otherwise, returns true.
 * @private
 */
FileTransferController.prototype.isDocumentWideEvent_ = function() {
  return this.document_.activeElement.nodeName.toLowerCase() !== 'input' ||
      this.document_.activeElement.type.toLowerCase() !== 'text';
};

/**
 * @param {boolean} isMove True for move operation.
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onCutOrCopy_ = function(isMove, event) {
  if (!this.isDocumentWideEvent_() ||
      !this.canCutOrCopy_(isMove)) {
    return;
  }

  event.preventDefault();

  var clipboardData = assert(event.clipboardData);
  var effectAllowed = isMove ? 'move' : 'copy';

  // If current focus is on DirectoryTree, write selected item of DirectoryTree
  // to system clipboard.
  if (document.activeElement instanceof DirectoryTree) {
    this.cutOrCopyFromDirectoryTree(
        document.activeElement, clipboardData, effectAllowed);
    return;
  }

  // If current focus is not on DirectoryTree, write the current selection in
  // the list to system clipboard.
  this.cutOrCopy_(clipboardData, effectAllowed);
  this.blinkSelection_();
};

/**
 * Performs cut or copy operation dispatched from directory tree.
 * @param {!DirectoryTree} directoryTree
 * @param {!ClipboardData} clipboardData
 * @param {string} effectAllowed
 */
FileTransferController.prototype.cutOrCopyFromDirectoryTree = function(
    directoryTree, clipboardData, effectAllowed) {
  var selectedItem = document.activeElement.selectedItem;
  if (selectedItem === null)
    return;

  var entry = selectedItem.entry;

  var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
  if (!volumeInfo)
    return;

  // When this value is false, we cannot copy between different sources.
  var missingFileContents =
      volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE &&
      this.volumeManager_.getDriveConnectionState().type ===
          VolumeManagerCommon.DriveConnectionType.OFFLINE;

  this.appendCutOrCopyInfo_(clipboardData, effectAllowed, volumeInfo, [entry],
      missingFileContents);
};

/**
 * @param {boolean} isMove True for move operation.
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onBeforeCutOrCopy_ = function(isMove, event) {
  if (!this.isDocumentWideEvent_())
    return;

  // queryCommandEnabled returns true if event.defaultPrevented is true.
  if (this.canCutOrCopy_(isMove))
    event.preventDefault();
};

/**
 * @param {boolean} isMove True for move operation.
 * @return {boolean}
 * @private
 */
FileTransferController.prototype.canCutOrCopy_ = function(isMove) {
  var command = isMove ? this.cutCommand_ : this.copyCommand_;
  command.setHidden(false);

  if (document.activeElement instanceof DirectoryTree) {
    var selectedItem = document.activeElement.selectedItem;
    if (!selectedItem)
      return false;

    if (!this.shouldShowCommandFor_(selectedItem.entry)) {
      command.setHidden(true);
      return false;
    }

    // Cut is unavailable on Team Drive roots.
    if (util.isTeamDriveRoot(selectedItem.entry)) {
      return false;
    }

    var metadata = this.metadataModel_.getCache(
        [selectedItem.entry], ['canCopy', 'canDelete']);
    assert(metadata.length === 1);

    if (!isMove) {
      return metadata[0].canCopy !== false;
    }

    // We need to check source volume is writable for move operation.
    var volumeInfo = this.volumeManager_.getVolumeInfo(selectedItem.entry);
    return !volumeInfo.isReadOnly && metadata[0].canCopy !== false &&
        metadata[0].canDelete !== false;
  }

  return isMove ? this.canCutOrDrag_() : this.canCopyOrDrag_() ;
};

/**
 * @return {boolean} Returns true if some files are selected and all the file
 *     on drive is available to be copied. Otherwise, returns false.
 * @private
 */
FileTransferController.prototype.canCopyOrDrag_ = function() {
  if (!this.selectionHandler_.isAvailable())
    return false;
  if (this.selectionHandler_.selection.entries.length <= 0)
    return false;
  var entries = this.selectionHandler_.selection.entries;
  for (var i = 0; i < entries.length; i++) {
    if (util.isTeamDriveRoot(entries[i]))
      return false;
    // If selected entries are not in the same directory, we can't copy them by
    // a single operation at this moment.
    if (i > 0 && !util.isSiblingEntry(entries[0], entries[i]))
      return false;
  }
  // Check if canCopy is true or undefined, but not false (see
  // https://crbug.com/849999).
  return this.metadataModel_.getCache(entries, ['canCopy'])
      .every(item => item.canCopy !== false);
};

/**
 * @return {boolean} Returns true if the current directory is not read only.
 * @private
 */
FileTransferController.prototype.canCutOrDrag_ = function() {
  if (this.directoryModel_.isReadOnly() ||
      !this.selectionHandler_.isAvailable() ||
      this.selectionHandler_.selection.entries.length <= 0) {
    return false;
  }
  var entries = this.selectionHandler_.selection.entries;
  // All entries need the 'canDelete' permission.
  var metadata = this.metadataModel_.getCache(entries, ['canDelete']);
  if (metadata.some(item => item.canDelete === false)) {
    return false;
  }
  return true;
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onPaste_ = function(event) {
  // If the event has destDirectory property, paste files into the directory.
  // This occurs when the command fires from menu item 'Paste into folder'.
  var destination =
      event.destDirectory || this.directoryModel_.getCurrentDirEntry();

  // Need to update here since 'beforepaste' doesn't fire.
  if (!this.isDocumentWideEvent_() ||
      !this.canPasteOrDrop_(assert(event.clipboardData), destination)) {
    return;
  }
  event.preventDefault();
  this.paste(assert(event.clipboardData), destination).then(effect => {
    // On cut, we clear the clipboard after the file is pasted/moved so we don't
    // try to move/delete the original file again.
    if (effect === 'move') {
      this.simulateCommand_('cut', function(event) {
        event.preventDefault();
        event.clipboardData.setData('fs/clear', '');
      });
    }
  });
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onBeforePaste_ = function(event) {
  if (!this.isDocumentWideEvent_())
    return;
  // queryCommandEnabled returns true if event.defaultPrevented is true.
  if (this.canPasteOrDrop_(assert(event.clipboardData),
                           this.directoryModel_.getCurrentDirEntry())) {
    event.preventDefault();
  }
};

/**
 * @param {!ClipboardData} clipboardData Clipboard data object.
 * @param {DirectoryEntry|FilesAppEntry} destinationEntry Destination
 *    entry.
 * @return {boolean} Returns true if items stored in {@code clipboardData} can
 *     be pasted to {@code destinationEntry}. Otherwise, returns false.
 * @private
 */
FileTransferController.prototype.canPasteOrDrop_ =
    function(clipboardData, destinationEntry) {
  if (!destinationEntry)
    return false;
  var destinationLocationInfo =
      this.volumeManager_.getLocationInfo(destinationEntry);
  if (!destinationLocationInfo || destinationLocationInfo.isReadOnly)
    return false;
  if (destinationLocationInfo.volumeInfo &&
      destinationLocationInfo.volumeInfo.error)
    return false;
  if (!clipboardData.types || clipboardData.types.indexOf('fs/tag') === -1)
    return false;  // Unsupported type of content.

  // Copying between different sources requires all files to be available.
  if (this.getSourceRootURL_(
          clipboardData, this.getDragAndDropGlobalData_()) !==
          destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
      this.isMissingFileContents_(clipboardData))
    return false;

  // Destination entry needs the 'canAddChildren' permission.
  var metadata =
      this.metadataModel_.getCache([destinationEntry], ['canAddChildren']);
  if (metadata[0].canAddChildren === false) {
    return false;
  }

  return true;
};

/**
 * Execute paste command.
 *
 * @param {DirectoryEntry|FilesAppEntry} destinationEntry
 * @return {boolean}  Returns true, the paste is success. Otherwise, returns
 *     false.
 */
FileTransferController.prototype.queryPasteCommandEnabled = function(
    destinationEntry) {
  if (!this.isDocumentWideEvent_()) {
    return false;
  }

  // HACK(serya): return this.document_.queryCommandEnabled('paste')
  // should be used.
  var result;
  this.simulateCommand_('paste', function(event) {
    result = this.canPasteOrDrop_(event.clipboardData, destinationEntry);
  }.bind(this));
  return result;
};

/**
 * Allows to simulate commands to get access to clipboard.
 *
 * @param {string} command 'copy', 'cut' or 'paste'.
 * @param {function(Event)} handler Event handler.
 * @private
 */
FileTransferController.prototype.simulateCommand_ = function(command, handler) {
  var iframe = this.document_.querySelector('#command-dispatcher');
  var doc = iframe.contentDocument;
  doc.addEventListener(command, handler);
  doc.execCommand(command);
  doc.removeEventListener(command, handler);
};

/**
 * @private
 */
FileTransferController.prototype.onFileSelectionChanged_ = function() {
  this.preloadedThumbnailImagePromise_ = null;
};

/**
 * @private
 */
FileTransferController.prototype.onFileSelectionChangedThrottled_ = function() {
  // Remove file objects that are no longer in the selection.
  var asyncData = {};
  var entries = this.selectionHandler_.selection.entries;
  for (var i = 0; i < entries.length; i++) {
    var entryUrl = entries[i].toURL();
    if (entryUrl in this.selectedAsyncData_) {
      asyncData[entryUrl] = this.selectedAsyncData_[entryUrl];
    }
  }
  this.selectedAsyncData_ = asyncData;

  var fileEntries = [];
  for (var i = 0; i < entries.length; i++) {
    if (entries[i].isFile)
      fileEntries.push(entries[i]);
    if (!(entries[i].toURL() in asyncData)) {
      asyncData[entries[i].toURL()] = {externalFileUrl: '', file: null};
    }
  }
  var containsDirectory = this.selectionHandler_.selection.directoryCount > 0;

  // File object must be prepeared in advance for clipboard operations
  // (copy, paste and drag). DataTransfer object closes for write after
  // returning control from that handlers so they may not have
  // asynchronous operations.
  if (!containsDirectory) {
    for (var i = 0; i < fileEntries.length; i++) {
      (function(fileEntry) {
        if (!(asyncData[fileEntry.toURL()].file)) {
          fileEntry.file(function(file) {
            asyncData[fileEntry.toURL()].file = file;
          });
        }
      })(fileEntries[i]);
    }
  }

  if (entries.length === 1) {
    // For single selection, the dragged element is created in advance,
    // otherwise an image may not be loaded at the time the 'dragstart' event
    // comes.
    this.preloadThumbnailImage_(entries[0]);
  }

  this.metadataModel_.get(entries, ['externalFileUrl']).then(
      function(metadataList) {
        // |Copy| is the only menu item affected by allDriveFilesAvailable_.
        // It could be open right now, update its UI.
        this.copyCommand_.disabled =
            !this.canCutOrCopy_(false /* not move operation */);
        for (var i = 0; i < entries.length; i++) {
          if (entries[i].isFile) {
            asyncData[entries[i].toURL()].externalFileUrl =
                metadataList[i].externalFileUrl;
          }
        }
      }.bind(this));
};

/**
 * @param {!Event} event Drag event.
 * @param {Object<string>} dragAndDropData drag & drop data from
 *     getDragAndDropGlobalData_().
 * @param {DirectoryEntry|FilesAppEntry} destinationEntry Destination
 *     entry.
 * @return {DropEffectAndLabel} Returns the appropriate drop query type
 *     ('none', 'move' or copy') to the current modifiers status and the
 *     destination, as well as label message to describe why the operation is
 *     not allowed.
 * @private
 */
FileTransferController.prototype.selectDropEffect_ = function(
    event, dragAndDropData, destinationEntry) {
  if (!destinationEntry)
    return new DropEffectAndLabel(DropEffectType.NONE, null);
  var destinationLocationInfo =
      this.volumeManager_.getLocationInfo(destinationEntry);
  if (!destinationLocationInfo)
    return new DropEffectAndLabel(DropEffectType.NONE, null);
  if (destinationLocationInfo.volumeInfo &&
      destinationLocationInfo.volumeInfo.error)
    return new DropEffectAndLabel(DropEffectType.NONE, null);
  if (destinationLocationInfo.isReadOnly) {
    if (destinationLocationInfo.isSpecialSearchRoot) {
      // The location is a fake entry that corresponds to special search.
      return new DropEffectAndLabel(DropEffectType.NONE, null);
    }
    if (destinationLocationInfo.rootType ==
        VolumeManagerCommon.RootType.CROSTINI) {
      // The location is a the fake entry for crostini.  Start container.
      return new DropEffectAndLabel(
          DropEffectType.NONE, strf('OPENING_LINUX_FILES'));
    }
    if (destinationLocationInfo.volumeInfo.isReadOnlyRemovableDevice) {
      return new DropEffectAndLabel(DropEffectType.NONE,
                                    strf('DEVICE_WRITE_PROTECTED'));
    }
    // The disk device is not write-protected but read-only.
    // Currently, the only remaining possibility is that write access to
    // removable drives is restricted by device policy.
    return new DropEffectAndLabel(DropEffectType.NONE,
                                  strf('DEVICE_ACCESS_RESTRICTED'));
  }
  var destinationMetadata =
      this.metadataModel_.getCache([destinationEntry], ['canAddChildren']);
  if (destinationMetadata.length === 1 &&
      destinationMetadata[0].canAddChildren === false) {
    // TODO(sashab): Distinguish between copy/move operations and display
    // corresponding warning text here.
    return new DropEffectAndLabel(
        DropEffectType.NONE,
        strf('DROP_TARGET_FOLDER_NO_MOVE_PERMISSION', destinationEntry.name));
  }
  if (util.isDropEffectAllowed(event.dataTransfer.effectAllowed, 'move')) {
    if (!util.isDropEffectAllowed(event.dataTransfer.effectAllowed, 'copy'))
      return new DropEffectAndLabel(DropEffectType.MOVE, null);
    // TODO(mtomasz): Use volumeId instead of comparing roots, as soon as
    // volumeId gets unique.
    if (this.getSourceRootURL_(event.dataTransfer, dragAndDropData) ===
            destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
        !event.ctrlKey) {
      return new DropEffectAndLabel(DropEffectType.MOVE, null);
    }
    if (event.shiftKey) {
      return new DropEffectAndLabel(DropEffectType.MOVE, null);
    }
  }
  return new DropEffectAndLabel(DropEffectType.COPY, null);
};

/**
 * Blinks the selection. Used to give feedback when copying or cutting the
 * selection.
 * @private
 */
FileTransferController.prototype.blinkSelection_ = function() {
  var selection = this.selectionHandler_.selection;
  if (!selection || selection.totalCount == 0)
    return;

  var listItems = [];
  for (var i = 0; i < selection.entries.length; i++) {
    var selectedIndex = selection.indexes[i];
    var listItem =
        this.listContainer_.currentList.getListItemByIndex(selectedIndex);
    if (listItem) {
      listItem.classList.add('blink');
      listItems.push(listItem);
    }
  }

  setTimeout(function() {
    for (var i = 0; i < listItems.length; i++) {
      listItems[i].classList.remove('blink');
    }
  }, 100);
};
