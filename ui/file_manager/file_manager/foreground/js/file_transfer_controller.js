// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';

import {getDisallowedTransfers, startIOTask} from '../../common/js/api.js';
import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {FileType} from '../../common/js/file_type.js';
import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {getEnabledTrashVolumeURLs, isAllTrashEntries, TrashEntry} from '../../common/js/trash.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {DropEffectAndLabel, DropEffectType} from './drop_effect_and_label.js';
import {FileSelectionHandler} from './file_selection.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {Command} from './ui/command.js';
import {DirectoryItem, DirectoryTree} from './ui/directory_tree.js';
import {DragSelector} from './ui/drag_selector.js';
import {List} from './ui/list.js';
import {ListContainer} from './ui/list_container.js';
import {TreeItem} from './ui/tree.js';

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
const DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * The key under which we store if the file content is missing. This property
 * tells us if we are attemptint to use a drive file while Drive is
 * disconnected.
 */
const MISSING_FILE_CONTENTS = 'missingFileContents';

/**
 * The key under which we store the root of the file system of files on which
 * we operate. This allows us to set the correct drag effect.
 */
const SOURCE_ROOT_URL = 'sourceRootURL';

/**
 * @typedef {{file:?File, externalFileUrl:string}}
 */
let FileAsyncData;

export class FileTransferController {
  /**
   * @param {!Document} doc Owning document.
   * @param {!ListContainer} listContainer List container.
   * @param {!DirectoryTree} directoryTree Directory tree.
   * @param {function(boolean, !Array<string>): !Promise<boolean>}
   *     confirmationCallback called when operation requires user's
   *     confirmation. The operation will be executed if the return value
   *     resolved to true.
   * @param {!ProgressCenter} progressCenter To notify starting copy operation.
   * @param {!FileOperationManager} fileOperationManager File operation manager
   *     instance.
   * @param {!MetadataModel} metadataModel Metadata cache service.
   * @param {!DirectoryModel} directoryModel Directory model instance.
   * @param {!VolumeManager} volumeManager Volume manager instance.
   * @param {!FileSelectionHandler} selectionHandler Selection handler.
   * @param {!FilesToast} filesToast Files toast.
   */
  constructor(
      doc, listContainer, directoryTree, confirmationCallback, progressCenter,
      fileOperationManager, metadataModel, directoryModel, volumeManager,
      selectionHandler, filesToast) {
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
     * @private {!FilesToast}
     * @const
     */
    this.filesToast_ = filesToast;

    /**
     * The array of pending task ID.
     * @type {Array<string>}
     */
    this.pendingTaskIds = [];

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
     * @private {!Command}
     * @const
     */
    this.copyCommand_ = /** @type {!Command} */ (
        queryRequiredElement('command#copy', assert(this.document_.body)));

    /**
     * @private {!Command}
     * @const
     */
    this.cutCommand_ = /** @type {!Command} */ (
        queryRequiredElement('command#cut', assert(this.document_.body)));

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
   * @param {!List} list Items in the list will be draggable.
   * @private
   */
  attachDragSource_(list) {
    list.style.webkitUserDrag = 'element';
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
    list.addEventListener('touchstart', this.onTouchStart_.bind(this));
    list.ownerDocument.addEventListener(
        'touchend', this.onTouchEnd_.bind(this), true);
    list.ownerDocument.addEventListener(
        'touchcancel', this.onTouchEnd_.bind(this), true);
  }

  /**
   * @param {!List} list List itself and its directory items will could
   *                          be drop target.
   * @param {boolean=} opt_onlyIntoDirectories If true only directory list
   *     items could be drop targets. Otherwise any other place of the list
   *     accetps files (putting it into the current directory).
   * @private
   */
  attachFileListDropTarget_(list, opt_onlyIntoDirectories) {
    list.addEventListener(
        'dragover',
        this.onDragOver_.bind(this, !!opt_onlyIntoDirectories, list));
    list.addEventListener(
        'dragenter', this.onDragEnterFileList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener(
        'drop', this.onDrop_.bind(this, !!opt_onlyIntoDirectories));
  }

  /**
   * @param {!DirectoryTree} tree Its sub items will could be drop target.
   * @private
   */
  attachTreeDropTarget_(tree) {
    tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
    tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
    tree.addEventListener('dragleave', this.onDragLeave_.bind(this, tree));
    tree.addEventListener('drop', this.onDrop_.bind(this, true));
  }

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   * @private
   */
  attachCopyPasteHandlers_() {
    this.document_.addEventListener(
        'beforecopy',
        this.onBeforeCutOrCopy_.bind(this, false /* not move operation */));
    this.document_.addEventListener(
        'copy', this.onCutOrCopy_.bind(this, false /* not move operation */));
    this.document_.addEventListener(
        'beforecut',
        this.onBeforeCutOrCopy_.bind(this, true /* move operation */));
    this.document_.addEventListener(
        'cut', this.onCutOrCopy_.bind(this, true /* move operation */));
    this.document_.addEventListener(
        'beforepaste', this.onBeforePaste_.bind(this));
    this.document_.addEventListener('paste', this.onPaste_.bind(this));
  }

  /**
   * Write the current selection to system clipboard.
   *
   * @param {DataTransfer} clipboardData DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |clipboardData.effectAllowed| property.
   * @private
   */
  cutOrCopy_(clipboardData, effectAllowed) {
    const currentDirEntry = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirEntry) {
      return;
    }
    let entry = currentDirEntry;
    if (util.isRecentRoot(currentDirEntry)) {
      entry = this.selectionHandler_.selection.entries[0];
    } else if (util.isTrashRoot(currentDirEntry)) {
      // In the event the entry resides in the Trash root, delegate to the item
      // in .Trash/files to get the source filesystem.
      const trashEntry = /** @type {TrashEntry|Entry} */ (
          this.selectionHandler_.selection.entries[0]);
      entry = trashEntry.filesEntry;
    }
    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    if (!volumeInfo) {
      return;
    }

    this.appendCutOrCopyInfo_(
        clipboardData, effectAllowed, volumeInfo,
        this.selectionHandler_.selection.entries,
        !this.selectionHandler_.isAvailable());
    this.appendUriList_(
        clipboardData, this.selectionHandler_.selection.entries);
  }

  /**
   * Appends copy or cut information of |entries| to |clipboardData|.
   * @param {DataTransfer} clipboardData DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |clipboardData.effectAllowed| property.
   * @param {!VolumeInfo} sourceVolumeInfo
   * @param {!Array<!Entry>} entries
   * @param {boolean} missingFileContents
   * @private
   */
  appendCutOrCopyInfo_(
      clipboardData, effectAllowed, sourceVolumeInfo, entries,
      missingFileContents) {
    // Tag to check it's filemanager data.
    clipboardData.setData('fs/tag', 'filemanager-data');
    clipboardData.setData(
        `fs/${SOURCE_ROOT_URL}`, sourceVolumeInfo.fileSystem.root.toURL());

    // In the event a cut event has begun from the TrashRoot, the sources should
    // be delegated to the underlying files to ensure any validation done
    // onDrop_ (e.g. DLP scanning) is done on the actual file.
    if (entries.every(util.isTrashEntry)) {
      entries = entries.map(e => {
        const trashEntry = /** @type {TrashEntry|Entry} */ (e);
        return trashEntry.filesEntry;
      });
    }

    const sourceURLs = util.entriesToURLs(entries);
    clipboardData.setData('fs/sources', sourceURLs.join('\n'));

    clipboardData.effectAllowed = effectAllowed;
    clipboardData.setData('fs/effectallowed', effectAllowed);

    clipboardData.setData(
        `fs/${MISSING_FILE_CONTENTS}`, missingFileContents.toString());
  }

  /**
   * Appends uri-list of |entries| to |clipboardData|.
   * @param {DataTransfer} clipboardData ClipboardData from the event.
   * @param {!Array<!Entry>} entries
   * @private
   */
  appendUriList_(clipboardData, entries) {
    let externalFileUrl;

    for (let i = 0; i < entries.length; i++) {
      const url = entries[i].toURL();
      if (!this.selectedAsyncData_[url]) {
        continue;
      }
      if (this.selectedAsyncData_[url].file) {
        clipboardData.items.add(assert(this.selectedAsyncData_[url].file));
      }
      if (!externalFileUrl) {
        externalFileUrl = this.selectedAsyncData_[url].externalFileUrl;
      }
    }

    if (externalFileUrl) {
      clipboardData.setData('text/uri-list', externalFileUrl);
    }
  }

  /**
   * @return {Object<string>} Drag and drop global data object.
   * @private
   */
  getDragAndDropGlobalData_() {
    const storage = window.localStorage;
    const sourceRootURL =
        storage.getItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`);
    const missingFileContents = storage.getItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`);
    if (sourceRootURL !== null && missingFileContents !== null) {
      return {sourceRootURL, missingFileContents};
    }
    return null;
  }

  /**
   * Extracts source root URL from the |clipboardData| or |dragAndDropData|
   * object.
   *
   * @param {!DataTransfer} clipboardData DataTransfer object from the event.
   * @param {Object<string>} dragAndDropData The drag and drop data from
   *     getDragAndDropGlobalData_().
   * @return {string} URL or an empty string (if unknown).
   * @private
   */
  getSourceRootURL_(clipboardData, dragAndDropData) {
    const sourceRootURL = clipboardData.getData(`fs/${SOURCE_ROOT_URL}`);
    if (sourceRootURL) {
      return sourceRootURL;
    }

    // |clipboardData| in protected mode.
    if (dragAndDropData) {
      return dragAndDropData.sourceRootURL;
    }

    // Unknown source.
    return '';
  }

  /**
   * @param {!DataTransfer} clipboardData DataTransfer object from the event.
   * @return {boolean} Returns true when missing some file contents.
   * @private
   */
  isMissingFileContents_(clipboardData) {
    let data = clipboardData.getData(`fs/${MISSING_FILE_CONTENTS}`);
    if (!data) {
      // |clipboardData| in protected mode.
      const globalData = this.getDragAndDropGlobalData_();
      if (globalData) {
        data = globalData.missingFileContents;
      }
    }
    return data === 'true';
  }

  /**
   * Calls executePaste with |pastePlan| if paste is allowed by Data Leak
   * Prevention policy. If paste is not allowed, it shows a toast to the
   * user.
   *
   * @param {!FileTransferController.PastePlan} pastePlan
   * @return {!Promise<string>} Either "copy", "move", "user-cancelled" or
   *     "dlp-aborted".
   * @private
   */
  async executePasteIfAllowed_(pastePlan) {
    const sourceEntries = await pastePlan.resolveEntries();
    let disallowedTransfers = [];
    try {
      if (util.isDlpEnabled()) {
        const destinationDir =
            /** @type{!DirectoryEntry} */ (
                assert(util.unwrapEntry(pastePlan.destinationEntry)));
        disallowedTransfers = await getDisallowedTransfers(
            sourceEntries, destinationDir, pastePlan.isMove);
      }
    } catch (error) {
      disallowedTransfers = [];
      console.warn(error);
    }

    if (disallowedTransfers && disallowedTransfers.length != 0) {
      let toastText;
      if (pastePlan.isMove) {
        if (disallowedTransfers.length == 1) {
          toastText = str('DLP_BLOCK_MOVE_TOAST');
        } else {
          toastText =
              strf('DLP_BLOCK_MOVE_TOAST_PLURAL', disallowedTransfers.length);
        }
      } else {
        if (disallowedTransfers.length == 1) {
          toastText = str('DLP_BLOCK_COPY_TOAST');
        } else {
          toastText =
              strf('DLP_BLOCK_COPY_TOAST_PLURAL', disallowedTransfers.length);
        }
      }
      this.filesToast_.show(toastText, {
        text: str('DLP_TOAST_BUTTON_LABEL'),
        callback: () => {
          util.visitURL(
              'https://support.google.com/chrome/a/?p=chromeos_datacontrols');
        },
      });
      return 'dlp-blocked';
    }
    if (sourceEntries.length == 0) {
      // This can happen when copied files were deleted before pasting
      // them. We execute the plan as-is, so as to share the post-copy
      // logic. This is basically same as getting empty by filtering
      // same-directory entries.
      return this.executePaste(pastePlan);
    }
    const confirmationType = pastePlan.getConfirmationType();
    if (confirmationType == FileTransferController.ConfirmationType.NONE) {
      return this.executePaste(pastePlan);
    }
    const messages = pastePlan.getConfirmationMessages(confirmationType);
    const userApproved =
        await this.confirmationCallback_(pastePlan.isMove, messages);
    if (!userApproved) {
      return 'user-cancelled';
    }
    return this.executePaste(pastePlan);
  }

  /**
   * Collects parameters of paste operation by the given command and the current
   * system clipboard.
   *
   * @param {!DataTransfer} clipboardData System data transfer object.
   * @param {DirectoryEntry=} opt_destinationEntry Paste destination.
   * @param {string=} opt_effect Desired drop/paste effect. Could be
   *     'move'|'copy' (default is copy). Ignored if conflicts with
   *     |clipboardData.effectAllowed|.
   * @return {!FileTransferController.PastePlan}
   */
  preparePaste(clipboardData, opt_destinationEntry, opt_effect) {
    const destinationEntry = assert(
        opt_destinationEntry ||
        /** @type {DirectoryEntry} */
        (this.directoryModel_.getCurrentDirEntry()));

    // When FilesApp does drag and drop to itself, it uses fs/sources to
    // populate sourceURLs, and it will resolve sourceEntries later using
    // webkitResolveLocalFileSystemURL().
    const sourceURLs = clipboardData.getData('fs/sources') ?
        clipboardData.getData('fs/sources').split('\n') :
        [];

    // When FilesApp is the paste target for other apps such as crostini,
    // the file URL is either not provided, or it is not compatible. We use
    // DataTransferItem.webkitGetAsEntry() to get the entry now.
    const sourceEntries = [];
    if (sourceURLs.length === 0) {
      for (let i = 0; i < clipboardData.items.length; i++) {
        if (clipboardData.items[i].kind === 'file') {
          const item = clipboardData.items[i];
          const entry = item.webkitGetAsEntry();
          if (entry !== null) {
            sourceEntries.push(entry);
          } else {
            // A File which does not resolve for webkitGetAsEntry() must be an
            // image drag drop from the browser. Write it to destination dir.
            this.fileOperationManager_.writeFile(
                assert(item.getAsFile()), destinationEntry);
          }
        }
      }
    }

    // effectAllowed set in copy/paste handlers stay uninitialized. DnD handlers
    // work fine.
    const effectAllowed = clipboardData.effectAllowed !== 'uninitialized' ?
        clipboardData.effectAllowed :
        clipboardData.getData('fs/effectallowed');
    const toMove = isDropEffectAllowed(effectAllowed, 'move') &&
        (!isDropEffectAllowed(effectAllowed, 'copy') || opt_effect === 'move');

    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo) {
      console.warn(
          'Failed to get destination location for ' + destinationEntry.toURL() +
          ' while attempting to paste files.');
    }
    assert(destinationLocationInfo);

    return new FileTransferController.PastePlan(
        sourceURLs, sourceEntries, destinationEntry, this.metadataModel_,
        toMove);
  }

  /**
   * Queue up a file copy operation based on the current system clipboard and
   * drag-and-drop global object.
   *
   * @param {!DataTransfer} clipboardData System data transfer object.
   * @param {DirectoryEntry=} opt_destinationEntry Paste destination.
   * @param {string=} opt_effect Desired drop/paste effect. Could be
   *     'move'|'copy' (default is copy). Ignored if conflicts with
   *     |clipboardData.effectAllowed|.
   * @return {!Promise<string>} Either "copy" or "move".
   */
  paste(clipboardData, opt_destinationEntry, opt_effect) {
    const pastePlan =
        this.preparePaste(clipboardData, opt_destinationEntry, opt_effect);

    return this.executePasteIfAllowed_(pastePlan);
  }

  /**
   * Queue up a file copy operation.
   *
   * @param {FileTransferController.PastePlan} pastePlan
   * @return {string} Either "copy" or "move".
   */
  executePaste(pastePlan) {
    const sourceURLs = pastePlan.sourceURLs;
    const toMove = pastePlan.isMove;
    const destinationEntry = pastePlan.destinationEntry;


    // Execute the IOTask in asynchronously.
    (async () => {
      try {
        const sourceEntries = await pastePlan.resolveEntries();
        const entries =
            await this.fileOperationManager_.filterSameDirectoryEntry(
                sourceEntries, destinationEntry, toMove);

        if (entries.length > 0) {
          if (isAllTrashEntries(entries, this.volumeManager_)) {
            await startIOTask(
                chrome.fileManagerPrivate.IOTaskType.RESTORE_TO_DESTINATION,
                entries, {destinationFolder: destinationEntry});
            return;
          }

          const taskType = toMove ? chrome.fileManagerPrivate.IOTaskType.MOVE :
                                    chrome.fileManagerPrivate.IOTaskType.COPY;
          // TODO(crbug/1290197): Start tracking the copy/move operation
          // starting here.
          const item = new ProgressCenterItem();
          item.type = /** @type {!ProgressItemType} */ (taskType);
          // Default to PENDING. It will be updated as matching IOTask events
          // are handled in FileOperationHandler::onIOTaskProgressStatus_.
          item.state = ProgressItemState.PENDING;
          item.itemCount = entries.length;
          item.remainingTime = 0;
          item.cancelCallback = () => {
            chrome.fileManagerPrivate.cancelIOTask(Number(item.id));
          };
          item.isDestinationDrive =
              this.volumeManager_.getVolumeInfo(destinationEntry).volumeType ===
              VolumeManagerCommon.VolumeType.DRIVE;
          item.id = String(await startIOTask(
              taskType, entries, {destinationFolder: destinationEntry}));
          this.progressCenter_.updateItem(item);
        }
      } catch (error) {
        console.warn(error.stack ? error.stack : error);
      } finally {
        // Publish source not found error item.
        for (let i = 0; i < pastePlan.failureUrls.length; i++) {
          const url = pastePlan.failureUrls[i];
          // Extract the file name.
          const fileName = decodeURIComponent(url.replace(/^.+\//, ''));
          const item = new ProgressCenterItem();
          item.id = `source-not-found-${url}`;
          item.state = ProgressItemState.ERROR;
          item.message = toMove ?
              strf('MOVE_SOURCE_NOT_FOUND_ERROR', fileName) :
              strf('COPY_SOURCE_NOT_FOUND_ERROR', fileName);
          this.progressCenter_.updateItem(item);
        }
      }
    })();

    return toMove ? 'move' : 'copy';
  }

  /**
   * Renders a drag-and-drop thumbnail.
   *
   * @return {!HTMLElement} Thumbnail element.
   * @private
   */
  renderThumbnail_() {
    const entry = this.selectionHandler_.selection.entries[0];
    const index = this.selectionHandler_.selection.indexes[0];
    const items = this.selectionHandler_.selection.entries.length;

    const container = /** @type {!HTMLElement} */ (
        this.document_.body.querySelector('#drag-container'));
    const multiple = items > 1 ? 'block' : 'none';
    container.innerHTML = `
      <div class='drag-box drag-multiple' style='display:${multiple}'></div>
      <div class='drag-box drag-contents'>
        <div class='detail-icon'></div><div class='label'>${entry.name}</div>
      </div>
      <div class='drag-bubble' style='display:${multiple}'>${items}</div>
    `;

    const icon = container.querySelector('.detail-icon');
    const thumbnail = this.listContainer_.currentView.getThumbnail(index);
    if (thumbnail) {
      icon.style.backgroundImage = thumbnail.style.backgroundImage;
      icon.style.backgroundSize = 'cover';
    } else {
      icon.setAttribute('file-type-icon', FileType.getIcon(entry));
    }

    return container;
  }

  /**
   * @param {!List} list Drop target list
   * @param {!Event} event A dragstart event of DOM.
   * @private
   */
  onDragStart_(list, event) {
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

    const dataTransfer = /** @type {DragEvent} */ (event).dataTransfer;

    const canCopy = this.canCopyOrDrag();
    const canCut = this.canCutOrDrag();
    if (canCopy || canCut) {
      if (canCopy && canCut) {
        this.cutOrCopy_(dataTransfer, 'all');
      } else if (canCopy) {
        this.cutOrCopy_(dataTransfer, 'copyLink');
      } else {
        this.cutOrCopy_(dataTransfer, 'move');
      }
    } else {
      event.preventDefault();
      return;
    }

    const thumbnail = {element: null, x: 0, y: 0};

    thumbnail.element = this.renderThumbnail_();
    if (this.document_.querySelector(':root[dir=rtl]')) {
      thumbnail.x = thumbnail.element.clientWidth * window.devicePixelRatio;
    }

    dataTransfer.setDragImage(thumbnail.element, thumbnail.x, thumbnail.y);

    const storage = window.localStorage;
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`,
        dataTransfer.getData(`fs/${SOURCE_ROOT_URL}`));
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`,
        dataTransfer.getData(`fs/${MISSING_FILE_CONTENTS}`));
  }

  /**
   * @param {!List} list Drop target list.
   * @param {!Event} event A dragend event of DOM.
   * @private
   */
  onDragEnd_(list, event) {
    // TODO(fukino): This is workaround for crbug.com/373125.
    // This should be removed after the bug is fixed.
    this.touching_ = false;

    const container = this.document_.body.querySelector('#drag-container');
    container.textContent = '';
    this.clearDropTarget_();
    const storage = window.localStorage;
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`);
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`);
  }

  /**
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directories.
   * @param {(!List|!DirectoryTree)} list Drop target list.
   * @param {Event} event A dragover event of DOM.
   * @private
   */
  onDragOver_(onlyIntoDirectories, list, event) {
    event.preventDefault();
    let entry = this.destinationEntry_;
    if (!entry && !onlyIntoDirectories) {
      entry = this.directoryModel_.getCurrentDirEntry();
    }

    const effectAndLabel =
        this.selectDropEffect_(event, this.getDragAndDropGlobalData_(), entry);
    event.dataTransfer.dropEffect = effectAndLabel.getDropEffect();
    event.preventDefault();
  }

  /**
   * @param {(!List|!DirectoryTree)} list Drop target list.
   * @param {!Event} event A dragenter event of DOM.
   * @private
   */
  onDragEnterFileList_(list, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.

    this.lastEnteredTarget_ = event.target;
    let item = list.getListItemAncestor(
        /** @type {HTMLElement} */ (event.target));
    item = item && list.isItem(item) ? item : null;

    if (item === this.dropTarget_) {
      return;
    }

    const entry = item && list.dataModel.item(item.listIndex);
    if (entry) {
      this.setDropTarget_(item, event.dataTransfer, entry);
    } else {
      this.clearDropTarget_();
    }
  }

  /**
   * @param {!DirectoryTree} tree Drop target tree.
   * @param {!Event} event A dragenter event of DOM.
   * @private
   */
  onDragEnterTree_(tree, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.

    if (!event.relatedTarget) {
      event.dataTransfer.dropEffect = 'move';
      return;
    }

    this.lastEnteredTarget_ = event.target;
    let item = event.target;
    while (item && !(item instanceof TreeItem)) {
      item = item.parentNode;
    }

    if (item === this.dropTarget_) {
      return;
    }

    const entry = item && item.entry;
    if (entry) {
      this.setDropTarget_(item, event.dataTransfer, entry);
    } else {
      this.clearDropTarget_();
    }
  }

  /**
   * @param {*} list Drop target list.
   * @param {Event} event A dragleave event of DOM.
   * @private
   */
  onDragLeave_(list, event) {
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

    // TODO(files-ng): dropLabel_ is not used in files-ng, remove it.
    if (this.dropLabel_) {
      this.dropLabel_.style.display = 'none';
    }
  }

  /**
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directories.
   * @param {!Event} event A dragleave event of DOM.
   * @private
   */
  async onDrop_(onlyIntoDirectories, event) {
    if (onlyIntoDirectories && !this.dropTarget_) {
      return;
    }
    const destinationEntry =
        this.destinationEntry_ || this.directoryModel_.getCurrentDirEntry();
    if (destinationEntry.rootType === VolumeManagerCommon.RootType.TRASH &&
        this.canTrashSelection_(destinationEntry, event.dataTransfer)) {
      event.preventDefault();
      const sourceURLs =
          (event.dataTransfer.getData('fs/sources') || '').split('\n');
      const {entries, failureUrls} =
          await FileTransferController.URLsToEntriesWithAccess(sourceURLs);

      // The list of entries should not be special entries (e.g. Camera, Linux
      // files) and should not already exist in Trash (i.e. you can't trash
      // something that's already trashed).
      const isModifiableAndNotInTrashRoot = entry => {
        return !util.isNonModifiable(this.volumeManager_, entry) &&
            !util.isTrashEntry(entry);
      };
      const canTrashEntries = entries && entries.length > 0 &&
          entries.every(isModifiableAndNotInTrashRoot);
      if (canTrashEntries && (!failureUrls || failureUrls.length === 0)) {
        startIOTask(
            chrome.fileManagerPrivate.IOTaskType.TRASH, entries,
            /*params=*/ {});
      }
      this.clearDropTarget_();
      return;
    }
    if (!this.canPasteOrDrop_(event.dataTransfer, destinationEntry)) {
      return;
    }
    event.preventDefault();
    this.paste(
        event.dataTransfer,
        /** @type {DirectoryEntry} */ (destinationEntry),
        this.selectDropEffect_(
                event, this.getDragAndDropGlobalData_(), destinationEntry)
            .getDropEffect());
    this.clearDropTarget_();
  }

  /**
   * Change to the drop target directory.
   * @private
   */
  changeToDropTargetDirectory_() {
    // Do custom action.
    if (this.dropTarget_ instanceof DirectoryItem) {
      /** @type {DirectoryItem} */ (this.dropTarget_).doDropTargetAction();
    }
    this.directoryModel_.changeDirectoryEntry(assert(this.destinationEntry_));
  }

  /**
   * Sets the drop target.
   *
   * @param {Element} domElement Target of the drop.
   * @param {!DataTransfer} clipboardData Data transfer object.
   * @param {!DirectoryEntry|!FakeEntry} destinationEntry Destination entry.
   * @private
   */
  setDropTarget_(domElement, clipboardData, destinationEntry) {
    if (this.dropTarget_ === domElement) {
      return;
    }

    // Remove the old drop target.
    this.clearDropTarget_();

    // Set the new drop target.
    this.dropTarget_ = domElement;
    if (!domElement || !destinationEntry.isDirectory) {
      return;
    }

    assert(destinationEntry.isDirectory);

    // Assume the destination directory won't accept this drop.
    domElement.classList.remove('accepts');
    domElement.classList.add('denies');

    // Disallow dropping a directory on itself.
    const entries = this.selectionHandler_.selection.entries;
    for (let i = 0; i < entries.length; i++) {
      if (util.isSameEntry(entries[i], destinationEntry)) {
        return;
      }
    }

    this.destinationEntry_ = destinationEntry;

    // Add accept classes if the directory can accept this drop.
    if (this.canPasteOrDrop_(clipboardData, destinationEntry)) {
      domElement.classList.remove('denies');
      domElement.classList.add('accepts');
    }

    // Change directory immediately if it's a fake entry for Crostini.
    if (destinationEntry.rootType === VolumeManagerCommon.RootType.CROSTINI) {
      this.changeToDropTargetDirectory_();
      return;
    }

    // Change to the directory after the drag target hover time out.
    const navigate = this.changeToDropTargetDirectory_.bind(this);
    this.navigateTimer_ = setTimeout(navigate, this.dragTargetHoverTime_());
  }

  /**
   * Return the drag target hover time in milliseconds.
   *
   * @private
   * @return {number}
   */
  dragTargetHoverTime_() {
    return window.IN_TEST ? 500 : 2000;
  }

  /**
   * Handles touch start.
   * @private
   */
  onTouchStart_() {
    this.touching_ = true;
  }

  /**
   * Handles touch end.
   * @private
   */
  onTouchEnd_() {
    // TODO(fukino): We have to check if event.touches.length be 0 to support
    // multi-touch operations, but event.touches has incorrect value by a bug
    // (crbug.com/373125).
    // After the bug is fixed, we should check event.touches.
    this.touching_ = false;
  }

  /**
   * Clears the drop target.
   * @private
   */
  clearDropTarget_() {
    if (this.dropTarget_) {
      this.dropTarget_.classList.remove('accepts', 'denies');
    }

    this.dropTarget_ = null;
    this.destinationEntry_ = null;

    if (this.navigateTimer_) {
      clearTimeout(this.navigateTimer_);
      this.navigateTimer_ = 0;
    }
  }

  /**
   * addEventListener only accepts callback that receives base class Event,
   * this forces clipboard event handlers to cast event to ClipboardEvent to
   * be able to use |clipboard| member.
   * @param {Event} event
   * @return {!DataTransfer}
   * @private
   */
  getClipboardData_(event) {
    const clipboardEvent = /** @type {ClipboardEvent} */ (event);
    return assert(clipboardEvent.clipboardData);
  }

  /**
   * @return {boolean} Returns false if {@code <input type="text"> or
   *     <cr-input>} element is currently active. Otherwise, returns true.
   * @private
   */
  isDocumentWideEvent_() {
    const element = this.document_.activeElement;
    const tagName = this.document_.activeElement.nodeName.toLowerCase();

    return !(
        (tagName === 'input' && element.type === 'text') ||
        tagName === 'cr-input');
  }

  /**
   * @param {boolean} isMove True for move operation.
   * @param {!Event} event
   * @private
   */
  onCutOrCopy_(isMove, event) {
    if (!this.isDocumentWideEvent_() || !this.canCutOrCopy_(isMove)) {
      return;
    }

    event.preventDefault();

    const clipboardData = this.getClipboardData_(event);
    const effectAllowed = isMove ? 'move' : 'copy';

    // If current focus is on DirectoryTree, write selected item of
    // DirectoryTree to system clipboard.
    if (document.activeElement instanceof DirectoryTree) {
      this.cutOrCopyFromDirectoryTree(
          document.activeElement, clipboardData, effectAllowed);
      return;
    }

    // If current focus is not on DirectoryTree, write the current selection in
    // the list to system clipboard.
    this.cutOrCopy_(clipboardData, effectAllowed);
    this.blinkSelection_();
  }

  /**
   * Performs cut or copy operation dispatched from directory tree.
   * @param {!DirectoryTree} directoryTree
   * @param {!DataTransfer} clipboardData
   * @param {string} effectAllowed
   */
  cutOrCopyFromDirectoryTree(directoryTree, clipboardData, effectAllowed) {
    const selectedItem = document.activeElement.selectedItem;
    if (selectedItem === null) {
      return;
    }

    const entry = selectedItem.entry;

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    if (!volumeInfo) {
      return;
    }

    // When this value is false, we cannot copy between different sources.
    const missingFileContents =
        volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE &&
        this.volumeManager_.getDriveConnectionState().type ===
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE;

    this.appendCutOrCopyInfo_(
        clipboardData, effectAllowed, volumeInfo, [entry], missingFileContents);
  }

  /**
   * @param {boolean} isMove True for move operation.
   * @param {!Event} event
   * @private
   */
  onBeforeCutOrCopy_(isMove, event) {
    if (!this.isDocumentWideEvent_()) {
      return;
    }

    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCutOrCopy_(isMove)) {
      event.preventDefault();
    }
  }

  /**
   * @param {boolean} isMove True for move operation.
   * @return {boolean}
   * @private
   */
  canCutOrCopy_(isMove) {
    const command = isMove ? this.cutCommand_ : this.copyCommand_;
    command.canExecuteChange(this.document_.activeElement);
    return !command.disabled;
  }

  /**
   * @return {boolean} Returns true if some files are selected and all the file
   *     on drive is available to be copied. Otherwise, returns false.
   * @public
   */
  canCopyOrDrag() {
    if (!this.selectionHandler_.isAvailable()) {
      return false;
    }
    if (this.selectionHandler_.selection.entries.length <= 0) {
      return false;
    }
    // Trash entries are only allowed to be restored which is analogous to a
    // cut event, so disallow the copy.
    if (this.selectionHandler_.selection.entries.every(util.isTrashEntry)) {
      return false;
    }
    const entries = this.selectionHandler_.selection.entries;
    for (let i = 0; i < entries.length; i++) {
      if (util.isTeamDriveRoot(entries[i])) {
        return false;
      }
      // If selected entries are not in the same directory, we can't copy them
      // by a single operation at this moment.
      if (i > 0 && !util.isSiblingEntry(entries[0], entries[i])) {
        return false;
      }
    }
    // Check if canCopy is true or undefined, but not false (see
    // https://crbug.com/849999).
    return this.metadataModel_.getCache(entries, ['canCopy'])
        .every(item => item.canCopy !== false);
  }

  /**
   * @return {boolean} Returns true if the current directory is not read only,
   *     or any of the selected entries isn't read-only.
   * @public
   */
  canCutOrDrag() {
    if (this.directoryModel_.isReadOnly() ||
        !this.selectionHandler_.isAvailable() ||
        this.selectionHandler_.selection.entries.length <= 0) {
      return false;
    }
    const entries = this.selectionHandler_.selection.entries;
    // All entries need the 'canDelete' permission.
    const metadata = this.metadataModel_.getCache(entries, ['canDelete']);
    if (metadata.some(item => item.canDelete === false)) {
      return false;
    }

    for (let i = 0; i < entries.length; i++) {
      if (util.isNonModifiable(this.volumeManager_, entries[i])) {
        return false;
      }
    }

    return true;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onPaste_(event) {
    // If the event has destDirectory property, paste files into the directory.
    // This occurs when the command fires from menu item 'Paste into folder'.
    const destination =
        event.destDirectory || this.directoryModel_.getCurrentDirEntry();

    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_() ||
        !this.canPasteOrDrop_(this.getClipboardData_(event), destination)) {
      return;
    }
    event.preventDefault();
    this.paste(this.getClipboardData_(event), destination).then(effect => {
      // On cut, we clear the clipboard after the file is pasted/moved so we
      // don't try to move/delete the original file again.
      if (effect === 'move') {
        this.simulateCommand_('cut', event => {
          event.preventDefault();
          event.clipboardData.setData('fs/clear', '');
        });
      }
    });
  }

  /**
   * @param {!Event} event
   * @private
   */
  onBeforePaste_(event) {
    if (!this.isDocumentWideEvent_()) {
      return;
    }
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canPasteOrDrop_(
            this.getClipboardData_(event),
            this.directoryModel_.getCurrentDirEntry())) {
      event.preventDefault();
    }
  }

  /**
   * @param {DataTransfer} clipboardData Data transfer object.
   * @param {DirectoryEntry|FilesAppEntry} destinationEntry Destination
   *    entry.
   * @return {boolean} Returns true if items stored in {@code clipboardData} can
   *     be pasted to {@code destinationEntry}. Otherwise, returns false.
   * @private
   */
  canPasteOrDrop_(clipboardData, destinationEntry) {
    if (!clipboardData) {
      return false;
    }

    if (!destinationEntry) {
      return false;
    }

    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo || destinationLocationInfo.isReadOnly) {
      return false;
    }

    // Recent isn't read-only, but it doesn't support paste/drop.
    if (destinationLocationInfo.rootType ===
        VolumeManagerCommon.RootType.RECENT) {
      return false;
    }

    if (destinationLocationInfo.volumeInfo &&
        destinationLocationInfo.volumeInfo.error) {
      return false;
    }

    // DataTransfer type will be 'fs/tag' when the source was FilesApp or exo,
    // or 'Files' when the source was any other app.
    const types = clipboardData.types;
    if (!types || !(types.includes('fs/tag') || types.includes('Files'))) {
      return false;  // Unsupported type of content.
    }

    // A drop on the Trash root should always perform a "Send to Trash"
    // operation.
    if (destinationLocationInfo.rootType ===
        VolumeManagerCommon.RootType.TRASH) {
      return this.canTrashSelection_(destinationLocationInfo, clipboardData);
    }

    const sourceUrls = (clipboardData.getData('fs/sources') || '').split('\n');
    if (this.getSourceRootURL_(
            clipboardData, this.getDragAndDropGlobalData_()) !==
        destinationLocationInfo.volumeInfo.fileSystem.root.toURL()) {
      // Copying between different sources requires all files to be available.
      if (this.isMissingFileContents_(clipboardData)) {
        return false;
      }

      // Block transferring hosted files between different sources in order to
      // prevent hosted files from being transferred outside of Drive. This is
      // done because hosted files aren't 'real' files, so it doesn't make sense
      // to allow a 'local' copy (e.g. in Downloads, or on a USB), where the
      // file can't be accessed offline (or necessarily accessed at all) by the
      // person who tries to open it. It also blocks copying hosted files to
      // other profiles, as the files would need to be shared in Drive first.
      if (sourceUrls.some(
              source => FileType.getTypeForName(source).type === 'hosted')) {
        return false;
      }
    }

    // If the destination is sub-tree of any of the sources paste isn't allowed.
    const addTrailingSlash = s => {
      if (!s.endsWith('/')) {
        s += '/';
      }
      return s;
    };
    const destinationUrl = addTrailingSlash(destinationEntry.toURL());
    if (sourceUrls.some(
            source => destinationUrl.startsWith(addTrailingSlash(source)))) {
      return false;
    }

    // Destination entry needs the 'canAddChildren' permission.
    const metadata =
        this.metadataModel_.getCache([destinationEntry], ['canAddChildren']);
    if (metadata[0].canAddChildren === false) {
      return false;
    }

    return true;
  }

  /**
   * Execute paste command.
   *
   * @param {DirectoryEntry|FilesAppEntry} destinationEntry
   * @return {boolean}  Returns true, the paste is success. Otherwise, returns
   *     false.
   */
  queryPasteCommandEnabled(destinationEntry) {
    if (!this.isDocumentWideEvent_()) {
      return false;
    }

    // HACK(serya): return this.document_.queryCommandEnabled('paste')
    // should be used.
    let result;
    this.simulateCommand_('paste', event => {
      result =
          this.canPasteOrDrop_(this.getClipboardData_(event), destinationEntry);
    });
    return result;
  }

  /**
   * Allows to simulate commands to get access to clipboard.
   *
   * @param {string} command 'copy', 'cut' or 'paste'.
   * @param {function(Event)} handler Event handler.
   * @private
   */
  simulateCommand_(command, handler) {
    const iframe = this.document_.body.querySelector('#command-dispatcher');
    const doc = iframe.contentDocument;
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  }

  /**
   * @private
   */
  onFileSelectionChangedThrottled_() {
    // Remove file objects that are no longer in the selection.
    const asyncData = {};
    const entries = this.selectionHandler_.selection.entries;
    for (let i = 0; i < entries.length; i++) {
      const entryUrl = entries[i].toURL();
      if (entryUrl in this.selectedAsyncData_) {
        asyncData[entryUrl] = this.selectedAsyncData_[entryUrl];
      }
    }
    this.selectedAsyncData_ = asyncData;

    const fileEntries = [];
    for (let i = 0; i < entries.length; i++) {
      if (entries[i].isFile) {
        fileEntries.push(entries[i]);
      }
      if (!(entries[i].toURL() in asyncData)) {
        asyncData[entries[i].toURL()] = {externalFileUrl: '', file: null};
      }
    }
    const containsDirectory =
        this.selectionHandler_.selection.directoryCount > 0;

    // File object must be prepeared in advance for clipboard operations
    // (copy, paste and drag). DataTransfer object closes for write after
    // returning control from that handlers so they may not have
    // asynchronous operations.
    if (!containsDirectory) {
      for (let i = 0; i < fileEntries.length; i++) {
        (fileEntry => {
          if (!(asyncData[fileEntry.toURL()].file)) {
            fileEntry.file(file => {
              asyncData[fileEntry.toURL()].file = file;
            });
          }
        })(fileEntries[i]);
      }
    }

    this.metadataModel_
        .get(entries, ['alternateUrl', 'externalFileUrl', 'hosted'])
        .then(metadataList => {
          for (let i = 0; i < entries.length; i++) {
            if (entries[i].isFile) {
              if (metadataList[i].hosted) {
                asyncData[entries[i].toURL()].externalFileUrl =
                    metadataList[i].alternateUrl;
              } else {
                asyncData[entries[i].toURL()].externalFileUrl =
                    metadataList[i].externalFileUrl;
              }
            }
          }
        });
  }

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
  selectDropEffect_(event, dragAndDropData, destinationEntry) {
    if (!destinationEntry) {
      return new DropEffectAndLabel(DropEffectType.NONE, null);
    }
    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo) {
      return new DropEffectAndLabel(DropEffectType.NONE, null);
    }
    if (destinationLocationInfo.volumeInfo &&
        destinationLocationInfo.volumeInfo.error) {
      return new DropEffectAndLabel(DropEffectType.NONE, null);
    }
    // Recent isn't read-only, but it doesn't support drop.
    if (destinationLocationInfo.rootType ===
        VolumeManagerCommon.RootType.RECENT) {
      return new DropEffectAndLabel(DropEffectType.NONE, null);
    }
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
      if (destinationLocationInfo.volumeInfo &&
          destinationLocationInfo.volumeInfo.isReadOnlyRemovableDevice) {
        return new DropEffectAndLabel(
            DropEffectType.NONE, strf('DEVICE_WRITE_PROTECTED'));
      }
      // The disk device is not write-protected but read-only.
      // Currently, the only remaining possibility is that write access to
      // removable drives is restricted by device policy.
      return new DropEffectAndLabel(
          DropEffectType.NONE, strf('DEVICE_ACCESS_RESTRICTED'));
    }
    const destinationMetadata =
        this.metadataModel_.getCache([destinationEntry], ['canAddChildren']);
    if (destinationMetadata.length === 1 &&
        destinationMetadata[0].canAddChildren === false) {
      // TODO(sashab): Distinguish between copy/move operations and display
      // corresponding warning text here.
      return new DropEffectAndLabel(
          DropEffectType.NONE,
          strf('DROP_TARGET_FOLDER_NO_MOVE_PERMISSION', destinationEntry.name));
    }
    // Files can be dragged onto the TrashRootEntry, but they must reside on a
    // volume that is trashable.
    if (destinationLocationInfo.rootType ===
        VolumeManagerCommon.RootType.TRASH) {
      const effect = (this.canTrashSelection_(
                         destinationLocationInfo, event.dataTransfer)) ?
          DropEffectType.MOVE :
          DropEffectType.NONE;
      return new DropEffectAndLabel(effect, null);
    }
    if (isDropEffectAllowed(event.dataTransfer.effectAllowed, 'move')) {
      if (!isDropEffectAllowed(event.dataTransfer.effectAllowed, 'copy')) {
        return new DropEffectAndLabel(DropEffectType.MOVE, null);
      }
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
  }

  /**
   * Identifies if the current selection can be sent to the trash. Items can be
   * dragged and dropped onto the TrashRootEntry, but they must all come from a
   * valid location that supports trash.
   * The URLs are compared against the volumes that are enabled for trashing.
   * This is to avoid blocking the drag drop operation with resolution of the
   * URLs to entries. This has the unfortunate side effect of not being able to
   * identify any non modifiable entries after a directory change but prior to
   * the drop event occurring.
   * @param {DirectoryEntry|FilesAppDirEntry|EntryLocation|null}
   *     destinationEntry The destination for the current selection.
   * @param {DataTransfer} clipboardData Data transfer object.
   * @returns {boolean} True if the selection can be trashed, false otherwise.
   * @private
   */
  canTrashSelection_(destinationEntry, clipboardData) {
    if (!util.isTrashEnabled() || !destinationEntry) {
      return false;
    }
    if (destinationEntry.rootType !== VolumeManagerCommon.RootType.TRASH) {
      return false;
    }
    if (!clipboardData) {
      return false;
    }
    const enabledTrashURLs = getEnabledTrashVolumeURLs(this.volumeManager_);
    // When the dragDrop event starts the selectionHandler_ contains the initial
    // selection, this is preferable to identify whether the selection is
    // available or not as the sources have resolved entries already.
    const {entries} = this.selectionHandler_.selection;
    if (entries && entries.length > 0) {
      for (const entry of entries) {
        if (util.isNonModifiable(this.volumeManager_, entry)) {
          return false;
        }
        const entryURL = entry.toURL();
        if (enabledTrashURLs.some(
                volumeURL => entryURL.startsWith(volumeURL))) {
          continue;
        }
        return false;
      }
      return true;
    }
    // If the selection is cleared the directory may have changed but the drag
    // event is still active. The only way to validate if the selection is
    // trashable now is to compare the `sourceRootURL` against the enabled trash
    // locations.
    // TODO(b/241517469): At this point the sourceRootURL may be on an enabled
    // location but the entry may not be trashable (e.g. Downloads and the
    // Camera folder). When the drop event occurs the URLs get resolved to
    // entries to ensure the operation can occur, but this may result in a move
    // operation showing as allowed when the drop doesn't accept it.
    const sourceRootURL =
        this.getSourceRootURL_(clipboardData, this.getDragAndDropGlobalData_());
    return enabledTrashURLs.some(
        volumeURL => sourceRootURL.startsWith(volumeURL));
  }

  /**
   * Blinks the selection. Used to give feedback when copying or cutting the
   * selection.
   * @private
   */
  blinkSelection_() {
    const selection = this.selectionHandler_.selection;
    if (!selection || selection.totalCount == 0) {
      return;
    }

    const listItems = [];
    for (let i = 0; i < selection.entries.length; i++) {
      const selectedIndex = selection.indexes[i];
      const listItem =
          this.listContainer_.currentList.getListItemByIndex(selectedIndex);
      if (listItem) {
        listItem.classList.add('blink');
        listItems.push(listItem);
      }
    }

    setTimeout(() => {
      for (let i = 0; i < listItems.length; i++) {
        listItems[i].classList.remove('blink');
      }
    }, 100);
  }
}

/**
 * Y coordinate of the label to describe drop action, relative to mouse cursor.
 *
 * @type {number}
 * @const
 * @private
 */
FileTransferController.DRAG_LABEL_Y_OFFSET_ = -32;

/**
 * Confirmation message types.
 *
 * @enum {string}
 */
FileTransferController.ConfirmationType = {
  NONE: 'none',
  COPY_TO_SHARED_DRIVE: 'copy_to_shared_drive',
  MOVE_TO_SHARED_DRIVE: 'move_to_shared_drive',
  MOVE_BETWEEN_SHARED_DRIVES: 'between_team_drives',
  MOVE_FROM_SHARED_DRIVE_TO_OTHER: 'move_from_team_drive_to_other',
  MOVE_FROM_OTHER_TO_SHARED_DRIVE: 'move_from_other_to_team_drive',
  COPY_FROM_OTHER_TO_SHARED_DRIVE: 'copy_from_other_to_team_drive',
};

/**
 * Container for defining a copy/move operation.
 */
FileTransferController.PastePlan = class {
  /**
   * @param {!Array<string>} sourceURLs URLs of source entries.
   * @param {!Array<!Entry>} sourceEntries Entries of source entries.
   * @param {!DirectoryEntry} destinationEntry Destination directory.
   * @param {!MetadataModel} metadataModel Metadata model instance.
   * @param {boolean} isMove true if move, false if copy.
   */
  constructor(
      sourceURLs, sourceEntries, destinationEntry, metadataModel, isMove) {
    /**
     * @type {!Array<string>}
     * @const
     */
    this.sourceURLs = sourceURLs;

    /**
     * @type {!Array<!Entry>}
     */
    this.sourceEntries = sourceEntries;

    /**
     * Any URLs from sourceURLs which failed resolving to into sourceEntries.
     * @type {!Array<string>}
     */
    this.failureUrls = [];

    /**
     * @type {!DirectoryEntry}
     */
    this.destinationEntry = destinationEntry;

    /**
     * @private {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @type {boolean}
     * @const
     */
    this.isMove = isMove;
  }

  /**
   * Resolves sourceEntries from sourceURLs if needed and returns them.
   *
   * @return {!Promise<!Array<!Entry>>}
   */
  async resolveEntries() {
    if (!this.sourceEntries.length) {
      const result =
          await FileTransferController.URLsToEntriesWithAccess(this.sourceURLs);
      this.sourceEntries = result.entries;
      this.failureUrls = result.failureUrls;
    }
    return this.sourceEntries;
  }

  /**
   * Obtains whether the planned operation requires user's confirmation, as well
   * as its type.
   *
   * @return {!FileTransferController.ConfirmationType} Type of the confirmation
   *     needed for the operation. If no confirmation is needed,
   *     FileTransferController.ConfirmationType.NONE will be returned.
   */
  getConfirmationType() {
    assert(this.sourceEntries.length !== 0);

    // Confirmation type for local drive.
    const sourceEntryCache =
        this.metadataModel_.getCache([this.sourceEntries[0]], ['shared']);
    const destinationEntryCache =
        this.metadataModel_.getCache([this.destinationEntry], ['shared']);

    // The shared property tells us whether an entry is shared on Drive, and is
    // potentially undefined.
    const isSharedSource = sourceEntryCache[0].shared === true;
    const isSharedDestination = destinationEntryCache[0].shared === true;

    // See crbug.com/731583#c20.
    if (!isSharedSource && isSharedDestination) {
      return this.isMove ?
          FileTransferController.ConfirmationType.MOVE_TO_SHARED_DRIVE :
          FileTransferController.ConfirmationType.COPY_TO_SHARED_DRIVE;
    }

    // Confirmation type for team drives.
    const source = {
      isTeamDrive: util.isSharedDriveEntry(this.sourceEntries[0]),
      teamDriveName: util.getTeamDriveName(this.sourceEntries[0]),
    };
    const destination = {
      isTeamDrive: util.isSharedDriveEntry(this.destinationEntry),
      teamDriveName: util.getTeamDriveName(this.destinationEntry),
    };
    if (this.isMove) {
      if (source.isTeamDrive) {
        if (destination.isTeamDrive) {
          if (source.teamDriveName == destination.teamDriveName) {
            return FileTransferController.ConfirmationType.NONE;
          } else {
            return FileTransferController.ConfirmationType
                .MOVE_BETWEEN_SHARED_DRIVES;
          }
        } else {
          return FileTransferController.ConfirmationType
              .MOVE_FROM_SHARED_DRIVE_TO_OTHER;
        }
      } else if (destination.isTeamDrive) {
        return FileTransferController.ConfirmationType
            .MOVE_FROM_OTHER_TO_SHARED_DRIVE;
      }
      return FileTransferController.ConfirmationType.NONE;
    } else {
      if (!destination.isTeamDrive) {
        return FileTransferController.ConfirmationType.NONE;
      }
      // Copying to Shared Drive.
      if (!(source.isTeamDrive &&
            source.teamDriveName == destination.teamDriveName)) {
        // This is not a copy within the same Shared Drive.
        return FileTransferController.ConfirmationType
            .COPY_FROM_OTHER_TO_SHARED_DRIVE;
      }
      return FileTransferController.ConfirmationType.NONE;
    }
  }

  /**
   * Composes a confirmation message for the given type.
   *
   * @param {FileTransferController.ConfirmationType} confirmationType
   * @return {!Array<string>} sentences for a confirmation dialog box.
   */
  getConfirmationMessages(confirmationType) {
    assert(this.sourceEntries.length != 0);
    const sourceName = util.getTeamDriveName(this.sourceEntries[0]);
    const destinationName = util.getTeamDriveName(this.destinationEntry);
    switch (confirmationType) {
      case FileTransferController.ConfirmationType.COPY_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_COPY_TO_SHARED_DRIVE',
            this.destinationEntry.fullPath.split('/').pop())];
      case FileTransferController.ConfirmationType.MOVE_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_MOVE_TO_SHARED_DRIVE',
            this.destinationEntry.fullPath.split('/').pop())];
      case FileTransferController.ConfirmationType.MOVE_BETWEEN_SHARED_DRIVES:
        return [
          strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName),
          strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName),
        ];
      // TODO(yamaguchi): notify ownership transfer if the two Shared Drives
      // belong to different domains.
      case FileTransferController.ConfirmationType
          .MOVE_FROM_SHARED_DRIVE_TO_OTHER:
        return [
          strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName),
          // TODO(yamaguchi): Warn if the operation moves at least one
          // directory to My Drive, as it's no undoable.
        ];
      case FileTransferController.ConfirmationType
          .MOVE_FROM_OTHER_TO_SHARED_DRIVE:
        return [strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS', destinationName)];
      case FileTransferController.ConfirmationType
          .COPY_FROM_OTHER_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName)];
    }
    assertNotReached('Invalid confirmation type: ' + confirmationType);
    return [];
  }
};

/**
 * Converts list of urls to list of Entries with granting R/W permissions to
 * them, which is essential when pasting files from a different profile.
 *
 * @param {!Array<string>} urls Urls to be converted.
 * @return {Promise} Promise fulfilled with the object that has entries property
 *     and failureUrls property. The promise is never rejected.
 */
FileTransferController.URLsToEntriesWithAccess = urls => {
  return new Promise((resolve, reject) => {
           chrome.fileManagerPrivate.grantAccess(
               urls, resolve.bind(null, undefined));
         })
      .then(() => {
        return util.URLsToEntries(urls);
      });
};


/**
 * Checks if the specified set of allowed effects contains the given effect.
 * See: http://www.w3.org/TR/html5/editing.html#the-datatransfer-interface
 *
 * @param {string} effectAllowed The string denoting the set of allowed effects.
 * @param {string} dropEffect The effect to be checked.
 * @return {boolean} True if |dropEffect| is included in |effectAllowed|.
 */
const isDropEffectAllowed = (effectAllowed, dropEffect) => {
  return effectAllowed === 'all' ||
      effectAllowed.toLowerCase().indexOf(dropEffect) !== -1;
};
