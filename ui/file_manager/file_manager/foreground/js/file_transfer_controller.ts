// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import type {ProgressCenter} from '../../background/js/progress_center.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getDirectory, getDisallowedTransfers, getFile, getParentEntry, grantAccess, startIOTask} from '../../common/js/api.js';
import {getFocusedTreeItem, htmlEscape, queryRequiredElement} from '../../common/js/dom_utils.js';
import {convertURLsToEntries, entriesToURLs, getRootType, getTeamDriveName, getTreeItemEntry, isNonModifiable, isRecentRoot, isSameEntry, isSharedDriveEntry, isSiblingEntry, isTeamDriveRoot, isTrashEntry, isTrashRoot, unwrapEntry} from '../../common/js/entry_utils.js';
import {getIcon, isEncrypted} from '../../common/js/file_type.js';
import {getFileTypeForName} from '../../common/js/file_types_base.js';
import type {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {isDlpEnabled} from '../../common/js/flags.js';
import {ProgressCenterItem, ProgressItemState} from '../../common/js/progress_center_common.js';
import {str, strf} from '../../common/js/translations.js';
import type {TrashEntry} from '../../common/js/trash.js';
import {getEnabledTrashVolumeURLs, isAllTrashEntries} from '../../common/js/trash.js';
import {FileErrorToDomError, visitURL} from '../../common/js/util.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import type {FileKey} from '../../state/state.js';
import {getFileData, getStore} from '../../state/store.js';
import type {XfTree} from '../../widgets/xf_tree.js';
import type {XfTreeItem} from '../../widgets/xf_tree_item.js';
import {isTreeItem, isXfTree} from '../../widgets/xf_tree_util.js';
import type {FilesToast} from '../elements/files_toast.js';

import type {DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import {EventType} from './file_selection.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {Command} from './ui/command.js';
import {DragSelector} from './ui/drag_selector.js';
import type {FileGrid} from './ui/file_grid.js';
import type {FileTableList} from './ui/file_table_list.js';
import type {Grid} from './ui/grid.js';
import type {List} from './ui/list.js';
import type {ListContainer} from './ui/list_container.js';
import type {ListItem} from './ui/list_item.js';

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
export const DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * The key under which we store if the file content is missing. This property
 * tells us if we are attempting to use a drive file while Drive is
 * disconnected.
 */
export const MISSING_FILE_CONTENTS = 'missingFileContents';

/**
 * The key under which we store the list of dragged files. This allows us to
 * set the correct drag effect.
 */
export const SOURCE_URLS = 'sources';

/**
 * The key under which we store the root of the file system of files on which
 * we operate. This allows us to set the correct drag effect.
 */
export const SOURCE_ROOT_URL = 'sourceRootURL';

/**
 * The key under which we store the flag denoting that the dragged file is
 * encrypted with Google Drive CSE. Given that decrypting of such files is not
 * implemented at the moment (May 2023), this allows us to unset the drag effect
 * when moving such a file outside Drive.
 */
export const ENCRYPTED = 'encrypted';

/**
 * Confirmation message types.
 */
enum TransferConfirmationType {
  NONE,
  COPY_TO_SHARED_DRIVE,
  MOVE_TO_SHARED_DRIVE,
  MOVE_BETWEEN_SHARED_DRIVES,
  MOVE_FROM_SHARED_DRIVE_TO_OTHER,
  MOVE_FROM_OTHER_TO_SHARED_DRIVE,
  COPY_FROM_OTHER_TO_SHARED_DRIVE,
}

enum DropEffectType {
  NONE = 'none',
  COPY = 'copy',
  MOVE = 'move',
  LINK = 'link',
}

export interface PasteWithDestDirectoryEvent extends ClipboardEvent {
  destDirectory: Entry|FilesAppEntry;
}

/**
 * ConfirmationCallback called when operation requires user's confirmation. The
 * operation will be executed if the return value resolved to true.
 */
type ConfirmationCallback = (isMove: boolean, messages: string[]) =>
    Promise<boolean>;

type FileAsyncData =
    Record<FileKey, {file?: File, externalFileUrl?: string}|null>;

interface DragAndDropGlobalData {
  sourceURLs: string;
  sourceRootURL: string;
  missingFileContents: string;
  encrypted: boolean;
}

/**
 * Extracts the `DataTransfer` from a generic event ensuring it's type asserted.
 */
const getClipboardData = (event: Event): DataTransfer|null => {
  const isClipboardEvent = (event: Event): event is ClipboardEvent =>
      'clipboardData' in event;
  return isClipboardEvent(event) ? event.clipboardData : null;
};

/**
 * The type of a file operation error.
 */
export enum FileOperationErrorType {
  UNEXPECTED_SOURCE_FILE = 0,
  TARGET_EXISTS = 1,
  FILESYSTEM_ERROR = 2,
}


/**
 * Error class used to report problems with a copy operation.
 * If the code is UNEXPECTED_SOURCE_FILE, data should be a path of the file.
 * If the code is TARGET_EXISTS, data should be the existing Entry.
 * If the code is FILESYSTEM_ERROR, data should be the FileError.
 */
class FileOperationError {
  /**
   * @param code Error type.
   * @param data Additional data.
   */
  constructor(
      public readonly code: FileOperationErrorType,
      public readonly data: string|Entry|DOMError) {}
}

/**
 * Resolves a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param root The root of the filesystem to search.
 * @param path The path to be resolved.
 * @return Promise fulfilled with the resolved entry, or rejected with
 *     FileError.
 */
export async function resolvePath(
    root: DirectoryEntry, path: string): Promise<Entry> {
  if (path === '' || path === '/') {
    return root;
  }
  try {
    return await getFile(root, path, {create: false});
  } catch (error: unknown) {
    const errorHasName = error && typeof error === 'object' && 'name' in error;
    if (errorHasName && error.name === FileErrorToDomError.TYPE_MISMATCH_ERR) {
      // Bah. It's a directory, ask again.
      return getDirectory(root, path, {create: false});
    }
    throw error;
  }
}

/**
 * Checks if an entry exists at |relativePath| in |dirEntry|.
 * If exists, tries to deduplicate the path by inserting parenthesized number,
 * such as " (1)", before the extension. If it still exists, tries the
 * deduplication again by increasing the number.
 * For example, suppose "file.txt" is given, "file.txt", "file (1).txt",
 * "file (2).txt", ... will be tried.
 *
 * @param dirEntry The target directory entry.
 * @param optSuccessCallback Callback run with the deduplicated path on success.
 * @param optErrorCallback Callback run on error.
 * @return  Promise fulfilled with available path.
 */
export async function deduplicatePath(
    dirEntry: DirectoryEntry, relativePath: string): Promise<string> {
  // Crack the path into three part. The parenthesized number (if exists)
  // will be replaced by incremented number for retry. For example, suppose
  // |relativePath| is "file (10).txt", the second check path will be
  // "file (11).txt".
  const match = /^(.*?)(?: \((\d+)\))?(\.[^.]*?)?$/.exec(relativePath)!;
  const prefix = match[1];
  const ext = match[3] || '';

  // Check to see if the target exists.
  async function customResolvePath(
      trialPath: string, copyNumber: number): Promise<string> {
    try {
      await resolvePath(dirEntry, trialPath);
      const newTrialPath = prefix + ' (' + copyNumber + ')' + ext;
      return await customResolvePath(newTrialPath, copyNumber + 1);
    } catch (error: unknown) {
      // We expect to be unable to resolve the target file, since
      // we're going to create it during the copy.  However, if the
      // resolve fails with anything other than NOT_FOUND, that's
      // trouble.
      const errorHasName =
          error && typeof error === 'object' && 'name' in error;
      if (errorHasName && error.name === FileErrorToDomError.NOT_FOUND_ERR) {
        return trialPath;
      }
      throw error;
    }
  }

  try {
    return await customResolvePath(relativePath, 1);
  } catch (error: unknown) {
    if (error instanceof Error) {
      throw error;
    }
    throw new FileOperationError(
        FileOperationErrorType.FILESYSTEM_ERROR, error as any);
  }
}

/**
 * Filters the entry in the same directory
 *
 * @param sourceEntries Entries of the source files.
 * @param targetEntry The destination entry of the target directory.
 * @param isMove True if the operation is "move", otherwise (i.e. if the
 *     operation is "copy") false.
 * @return Promise fulfilled with the filtered entry. This is not rejected.
 */
async function filterSameDirectoryEntry(
    sourceEntries: Entry[], targetEntry: DirectoryEntry|FakeEntry,
    isMove: boolean): Promise<Entry[]> {
  if (!isMove) {
    return sourceEntries;
  }

  // Check all file entries and keeps only those need sharing operation.
  async function processEntry(entry: Entry): Promise<Entry|null> {
    try {
      const inParentEntry = await getParentEntry(entry);
      return isSameEntry(inParentEntry, targetEntry) ? null : entry;
    } catch (error: unknown) {
      console.warn((error as any).stack || error);
      return null;
    }
  }

  // Call processEntry for each item of sourceEntries.
  const result = await Promise.all(sourceEntries.map(processEntry));

  // Remove null entries.
  return result.filter(entry => !!entry) as Entry[];
}

/**
 * Writes file to destination dir. This function is called when an image is
 * dragged from a web page. In this case there is no FileSystem Entry to copy
 * or move, just the JS File object with attached Blob. This operation does
 * not use EventRouter or queue the task since it is not possible to track
 * progress of the FileWriter.write().
 *
 * @param file The file entry to be written.
 * @param dir The destination directory to write to.
 */
export async function writeFile(
    file: File, dir: DirectoryEntry): Promise<FileEntry> {
  const name = await deduplicatePath(dir, file.name);
  return new Promise((resolve, reject) => {
    dir.getFile(name, {create: true, exclusive: true}, f => {
      f.createWriter(writer => {
        writer.onwriteend = () => resolve(f);
        writer.onerror = reject;
        writer.write(file);
      }, reject);
    }, reject);
  });
}

export class FileTransferController {
  /**
   * The array of the pending task IDs.
   */
  pendingTaskIds: string[] = [];

  /**
   * File objects for selected files.
   */
  private selectedAsyncData_: FileAsyncData = {};

  /**
   * Drag selector.
   */
  private dragSelector_ = new DragSelector();

  /**
   * Whether a user is touching the device or not.
   */
  private touching_ = false;

  private copyCommand_ =
      queryRequiredElement('command#copy', this.document_.body) as Command;

  private cutCommand_ =
      queryRequiredElement('command#cut', this.document_.body) as Command;

  private destinationEntry_: DirectoryEntry|FilesAppDirEntry|null = null;

  private lastEnteredTarget_: HTMLElement|null = null;

  private dropTarget_: Element|null = null;

  /**
   * The element for showing a label while dragging files.
   */
  private dropLabel_: HTMLElement|null = null;

  private navigateTimer_ = 0;

  constructor(
      private document_: Document, private listContainer_: ListContainer,
      directoryTree: XfTree,
      private confirmationCallback_: ConfirmationCallback,
      private progressCenter_: ProgressCenter,
      /**
       * Note: We use synchronous `getCache` method under assumption that fields
       * we request are already cached. See constants.js, specifically
       * LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES for list of fields
       * which are safe to use.
       */
      private metadataModel_: MetadataModel,
      private directoryModel_: DirectoryModel,
      private volumeManager_: VolumeManager,
      private selectionHandler_: FileSelectionHandler,
      private filesToast_: FilesToast) {
    // Register the events.
    this.selectionHandler_.addEventListener(
        EventType.CHANGE_THROTTLED,
        this.onFileSelectionChangedThrottled_.bind(this));
    this.attachDragSource_(this.listContainer_.table.list as FileTableList);
    this.attachFileListDropTarget_(this.listContainer_.table.list);
    this.attachDragSource_(this.listContainer_.grid);
    this.attachFileListDropTarget_(this.listContainer_.grid);
    this.attachTreeDropTarget_(directoryTree);
    this.attachCopyPasteHandlers_();

    // Allow to drag external files to the browser window.
    chrome.fileManagerPrivate.enableExternalFileScheme();
  }

  /**
   * Attaches items in the `list` that will be draggable.
   */
  private attachDragSource_(list: FileTableList|FileGrid) {
    if ('webkitUserDrag' in list.style) {
      list.style.webkitUserDrag = 'element';
    }
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this));
    list.addEventListener('touchstart', this.onTouchStart_.bind(this));
    list.ownerDocument.addEventListener(
        'touchend', this.onTouchEnd_.bind(this), true);
    list.ownerDocument.addEventListener(
        'touchcancel', this.onTouchEnd_.bind(this), true);
  }

  private attachFileListDropTarget_(list: List|Grid) {
    list.addEventListener('dragover', this.onDragOver_.bind(this, false, list));
    list.addEventListener(
        'dragenter', this.onDragEnterFileList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this));
    list.addEventListener('drop', this.onDrop_.bind(this, false));
  }

  private attachTreeDropTarget_(tree: XfTree) {
    tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
    tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
    tree.addEventListener('dragleave', this.onDragLeave_.bind(this));
    tree.addEventListener('drop', this.onDrop_.bind(this, true));
  }

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   */
  private attachCopyPasteHandlers_() {
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
        'onbeforepaste', this.onBeforePaste_.bind(this));
    this.document_.addEventListener('paste', this.onPaste_.bind(this));
  }

  /**
   * Write the current selection to system clipboard.
   */
  private cutOrCopy_(
      clipboardData: DataTransfer|null,
      effectAllowed: DataTransfer['effectAllowed']) {
    const currentDirEntry = this.directoryModel_.getCurrentDirEntry();
    if (!currentDirEntry) {
      return;
    }
    let entry: Entry|FilesAppEntry|FakeEntry|FilesAppDirEntry = currentDirEntry;
    if (isRecentRoot(currentDirEntry)) {
      entry = this.selectionHandler_.selection.entries[0]!;
    } else if (isTrashRoot(currentDirEntry)) {
      // In the event the entry resides in the Trash root, delegate to the item
      // in .Trash/files to get the source filesystem.
      const trashEntry =
          this.selectionHandler_.selection.entries[0] as TrashEntry;
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
    this.appendFiles_(clipboardData, this.selectionHandler_.selection.entries);
  }

  /**
   * Appends copy or cut information of |entries| to |clipboardData|.
   */
  private appendCutOrCopyInfo_(
      clipboardData: DataTransfer|null,
      effectAllowed: DataTransfer['effectAllowed'],
      sourceVolumeInfo: VolumeInfo, entries: Array<Entry|FilesAppEntry>,
      missingFileContents: boolean) {
    if (!clipboardData) {
      return;
    }
    // Tag to check it's filemanager data.
    clipboardData.setData('fs/tag', 'filemanager-data');
    clipboardData.setData(
        `fs/${SOURCE_ROOT_URL}`, sourceVolumeInfo.fileSystem.root.toURL());

    // In the event a cut event has begun from the TrashRoot, the sources should
    // be delegated to the underlying files to ensure any validation done
    // onDrop_ (e.g. DLP scanning) is done on the actual file.
    if (entries.every(isTrashEntry)) {
      entries = entries.map(e => (e as TrashEntry).filesEntry);
    }

    const encrypted =
        this.metadataModel_.getCache(entries, ['contentMimeType'])
            .every(
                (metadata, i) => entries[i] ?
                    isEncrypted(entries[i]!, metadata.contentMimeType) :
                    false);

    const sourceURLs = entriesToURLs(entries);
    clipboardData.setData('fs/sources', sourceURLs.join('\n'));

    clipboardData.effectAllowed = effectAllowed;
    clipboardData.setData('fs/effectallowed', effectAllowed);

    clipboardData.setData(`fs/${ENCRYPTED}`, encrypted.toString());
    clipboardData.setData(
        `fs/${MISSING_FILE_CONTENTS}`, missingFileContents.toString());
  }

  /**
   * Appends files of |entries| to |clipboardData|.
   */
  private appendFiles_(
      clipboardData: DataTransfer|null, entries: Array<Entry|FilesAppEntry>) {
    if (!clipboardData) {
      return;
    }
    for (let i = 0; i < entries.length; i++) {
      const url = entries[i]?.toURL();
      if (!url || !this.selectedAsyncData_[url]) {
        continue;
      }
      if (this.selectedAsyncData_[url]?.file) {
        clipboardData.items.add(this.selectedAsyncData_[url]!.file!);
      }
    }
  }

  private getDragAndDropGlobalData_(): DragAndDropGlobalData|null {
    const storage = window.localStorage;
    const sourceURLs =
        storage.getItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`);
    const sourceRootURL =
        storage.getItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`);
    const missingFileContents = storage.getItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`);
    const encrypted =
        storage.getItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${ENCRYPTED}`) === 'true';
    if (sourceURLs !== null && sourceRootURL !== null &&
        missingFileContents !== null) {
      return {sourceURLs, sourceRootURL, missingFileContents, encrypted};
    }
    return null;
  }

  /**
   * Extracts source root URL from the |clipboardData| or |dragAndDropData|
   * object.
   */
  private getSourceRootUrl_(
      clipboardData: DataTransfer,
      dragAndDropData: DragAndDropGlobalData|null) {
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

  private isMissingFileContents_(clipboardData: DataTransfer) {
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

  private isEncrypted_(clipboardData: DataTransfer) {
    const data = clipboardData.getData(`fs/${ENCRYPTED}`);
    if (data) {
      return data === 'true';
    }
    // |clipboardData| in protected mode.
    const globalData = this.getDragAndDropGlobalData_();
    if (globalData) {
      return globalData.encrypted;
    }
    return false;
  }
  /**
   * Calls executePaste with |pastePlan| if paste is allowed by Data Leak
   * Prevention policy. If paste is not allowed, it shows a toast to the
   * user.
   */
  private async executePasteIfAllowed_(pastePlan: PastePlan) {
    const sourceEntries = await pastePlan.resolveEntries();
    let disallowedTransfers: Entry[] = [];
    try {
      if (isDlpEnabled()) {
        const destinationDir = unwrapEntry(pastePlan.destinationEntry);
        disallowedTransfers = await getDisallowedTransfers(
            sourceEntries, destinationDir, pastePlan.isMove);
      }
    } catch (error) {
      disallowedTransfers = [];
      console.warn(error);
    }

    if (disallowedTransfers && disallowedTransfers.length !== 0) {
      let toastText;
      if (pastePlan.isMove) {
        if (disallowedTransfers.length === 1) {
          toastText = str('DLP_BLOCK_MOVE_TOAST');
        } else {
          toastText =
              strf('DLP_BLOCK_MOVE_TOAST_PLURAL', disallowedTransfers.length);
        }
      } else {
        if (disallowedTransfers.length === 1) {
          toastText = str('DLP_BLOCK_COPY_TOAST');
        } else {
          toastText =
              strf('DLP_BLOCK_COPY_TOAST_PLURAL', disallowedTransfers.length);
        }
      }
      this.filesToast_.show(toastText, {
        text: str('DLP_TOAST_BUTTON_LABEL'),
        callback: () => {
          visitURL(
              'https://support.google.com/chrome/a/?p=chromeos_datacontrols');
        },
      });
      return 'dlp-blocked';
    }
    if (sourceEntries.length === 0) {
      // This can happen when copied files were deleted before pasting
      // them. We execute the plan as-is, so as to share the post-copy
      // logic. This is basically same as getting empty by filtering
      // same-directory entries.
      return this.executePaste(pastePlan);
    }
    const confirmationType = pastePlan.getConfirmationType();
    if (confirmationType === TransferConfirmationType.NONE) {
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
   * @param writeFileFunc Used for unittest.
   */
  preparePaste(
      clipboardData: DataTransfer|null,
      destinationEntry?: FilesAppDirEntry|DirectoryEntry, effect?: string,
      writeFileFunc = writeFile) {
    destinationEntry =
        destinationEntry || this.directoryModel_.getCurrentDirEntry();
    assert(destinationEntry);

    // When FilesApp does drag and drop to itself, it uses fs/sources to
    // populate sourceURLs, and it will resolve sourceEntries later using
    // webkitResolveLocalFileSystemURL().
    const sourceURLs = clipboardData?.getData('fs/sources') ?
        clipboardData!.getData('fs/sources').split('\n') :
        [];

    // When FilesApp is the paste target for other apps such as crostini,
    // the file URL is either not provided, or it is not compatible. We use
    // DataTransferItem.webkitGetAsEntry() to get the entry now.
    const sourceEntries = [];
    if (sourceURLs.length === 0 && clipboardData?.items) {
      for (let i = 0; i < clipboardData.items.length; i++) {
        if (clipboardData.items[i]?.kind === 'file') {
          const item = clipboardData.items[i]!;
          const entry = item.webkitGetAsEntry();
          if (entry !== null) {
            sourceEntries.push(entry);
            continue;
          } else {
            // A File which does not resolve for webkitGetAsEntry() must be an
            // image drag drop from the browser. Write it to destination dir.
            writeFileFunc(
                item.getAsFile()!, destinationEntry as DirectoryEntry);
          }
        }
      }
    }

    // effectAllowed set in copy/paste handlers stay uninitialized. DnD handlers
    // work fine.
    const effectAllowed = clipboardData?.effectAllowed !== 'uninitialized' ?
        clipboardData?.effectAllowed as DataTransfer['effectAllowed'] :
        clipboardData.getData('fs/effectallowed') as
            DataTransfer['effectAllowed'];
    const toMove = isDropEffectAllowed(effectAllowed, 'move') &&
        (!isDropEffectAllowed(effectAllowed, 'copy') || effect === 'move');

    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo) {
      console.warn(
          'Failed to get destination location for ' + destinationEntry.toURL() +
          ' while attempting to paste files.');
    }
    assert(destinationLocationInfo);

    return new PastePlan(
        sourceURLs, sourceEntries, destinationEntry, this.metadataModel_,
        toMove);
  }

  /**
   * Queue up a file copy operation based on the current system clipboard and
   * drag-and-drop global object.
   */
  async paste(
      clipboardData: DataTransfer|null,
      destinationEntry?: FilesAppDirEntry|DirectoryEntry, effect?: string) {
    const pastePlan =
        this.preparePaste(clipboardData, destinationEntry, effect);

    return this.executePasteIfAllowed_(pastePlan);
  }

  /**
   * Queue up a file copy operation.
   */
  executePaste(pastePlan: PastePlan) {
    const toMove = pastePlan.isMove;
    const destinationEntry = pastePlan.destinationEntry as DirectoryEntry;

    // Execute the IOTask in asynchronously.
    (async () => {
      try {
        const sourceEntries = await pastePlan.resolveEntries();
        const entries = await filterSameDirectoryEntry(
            sourceEntries, destinationEntry, toMove);

        if (entries.length > 0) {
          if (isAllTrashEntries(entries, this.volumeManager_)) {
            await startIOTask(
                chrome.fileManagerPrivate.IoTaskType.RESTORE_TO_DESTINATION,
                entries, {destinationFolder: destinationEntry});
            return;
          }

          const taskType = toMove ? chrome.fileManagerPrivate.IoTaskType.MOVE :
                                    chrome.fileManagerPrivate.IoTaskType.COPY;
          await startIOTask(
              taskType, entries, {destinationFolder: destinationEntry});
        }
      } catch (error: any) {
        console.warn(error.stack ? error.stack : error);
      } finally {
        // Publish source not found error item.
        for (let i = 0; i < pastePlan.failureUrls.length; i++) {
          const url = pastePlan.failureUrls[i];
          if (!url) {
            continue;
          }
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
   */
  private renderThumbnail_() {
    const entry = this.selectionHandler_.selection.entries[0]!;
    const index = this.selectionHandler_.selection.indexes[0]!;
    const items = this.selectionHandler_.selection.entries.length;

    const container =
        this.document_.body.querySelector<HTMLDivElement>('#drag-container')!;
    const html = `
      ${items > 1 ? `<div class='drag-box drag-multiple'></div>` : ''}
      <div class='drag-box drag-contents'>
        <div class='detail-icon'></div>
        <div class='label'>${htmlEscape(entry.name)}</div>
      </div>
      ${items > 1 ? `<div class='drag-bubble'>${items}</div>` : ''}
    `;
    container.innerHTML = sanitizeInnerHtml(html, {attrs: ['class']});

    const icon = container.querySelector<HTMLElement>('.detail-icon')!;
    const thumbnail =
        this.listContainer_.currentView.getThumbnail(index) as HTMLElement;
    if (thumbnail) {
      icon.style.backgroundImage = thumbnail.style.backgroundImage;
      icon.style.backgroundSize = 'cover';
    } else {
      icon.setAttribute('file-type-icon', getIcon(entry));
    }

    return container;
  }

  private onDragStart_(list: FileGrid|FileTableList, event: MouseEvent) {
    // If renaming is in progress, drag operation should be used for selecting
    // substring of the text. So we don't drag files here.
    if ('currentEntry' in this.listContainer_.renameInput &&
        this.listContainer_.renameInput.currentEntry) {
      event.preventDefault();
      return;
    }

    // If this drag operation is initiated by mouse, check if we should start
    // selecting area.
    if (!this.touching_ && list.shouldStartDragSelection!(event)) {
      event.preventDefault();
      this.dragSelector_.startDragSelection(list, event);
      return;
    }

    // If the drag starts outside the files list on a touch device, cancel the
    // drag.
    if (this.touching_ && !list.hasDragHitElement(event)) {
      event.preventDefault();
      list.selectionModel!.unselectAll();
      return;
    }

    // Nothing selected.
    if (!this.selectionHandler_.selection.entries.length) {
      event.preventDefault();
      return;
    }

    const dataTransfer = (event as DragEvent).dataTransfer!;

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

    const thumbnailElement = this.renderThumbnail_();
    let thumbnailX = 0;
    if (this.document_.querySelector(':root[dir=rtl]')) {
      thumbnailX = thumbnailElement.clientWidth * window.devicePixelRatio;
    }

    dataTransfer.setDragImage(thumbnailElement, thumbnailX, /*y=*/ 0);

    const storage = window.localStorage;
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`,
        dataTransfer.getData(`fs/${SOURCE_URLS}`));
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`,
        dataTransfer.getData(`fs/${SOURCE_ROOT_URL}`));
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`,
        dataTransfer.getData(`fs/${MISSING_FILE_CONTENTS}`));
    storage.setItem(
        `${DRAG_AND_DROP_GLOBAL_DATA}.${ENCRYPTED}`,
        dataTransfer.getData(`fs/${ENCRYPTED}`));
  }

  private onDragEnd_() {
    // TODO(fukino): This is workaround for crbug.com/373125.
    // This should be removed after the bug is fixed.
    this.touching_ = false;

    const container = this.document_.body.querySelector('#drag-container')!;
    container.textContent = '';
    this.clearDropTarget_();
    const storage = window.localStorage;
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_URLS}`);
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${SOURCE_ROOT_URL}`);
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${MISSING_FILE_CONTENTS}`);
    storage.removeItem(`${DRAG_AND_DROP_GLOBAL_DATA}.${ENCRYPTED}`);
  }

  private onDragOver_(
      onlyIntoDirectories: boolean, _: List|XfTree, event: DragEvent) {
    event.preventDefault();
    let entry: DirectoryEntry|FilesAppDirEntry|null|undefined =
        this.destinationEntry_;
    if (!entry && !onlyIntoDirectories) {
      entry = this.directoryModel_.getCurrentDirEntry();
    }

    event.dataTransfer!.dropEffect =
        this.selectDropEffect_(event, this.getDragAndDropGlobalData_(), entry!);
    event.preventDefault();
  }

  private onDragEnterFileList_(list: List, event: DragEvent) {
    event.preventDefault();  // Required to prevent the cursor flicker.

    this.lastEnteredTarget_ = event.target as HTMLElement;
    let item: ListItem|null = list.getListItemAncestor(this.lastEnteredTarget_);
    item = item && list.isItem(item) ? item : null;

    if (item === this.dropTarget_) {
      return;
    }

    const entry = item && list.dataModel!.item(item.listIndex);
    if (entry && event.dataTransfer) {
      this.setDropTarget_(item, event.dataTransfer, entry);
    } else {
      this.clearDropTarget_();
    }
  }

  private onDragEnterTree_(_: XfTree, event: DragEvent) {
    event.preventDefault();  // Required to prevent the cursor flicker.

    if (!event.relatedTarget) {
      if (event.dataTransfer) {
        event.dataTransfer.dropEffect = 'move';
      }
      return;
    }

    this.lastEnteredTarget_ = event.target as HTMLElement;
    let item = event.target as Element;
    while (item && !(isTreeItem(item))) {
      item = item.parentNode as Element;
    }

    if (item === this.dropTarget_) {
      return;
    }

    const entry = getTreeItemEntry(item);

    if (item && entry && event.dataTransfer) {
      this.setDropTarget_(item, event.dataTransfer, entry as DirectoryEntry);
    } else {
      this.clearDropTarget_();
    }
  }

  private onDragLeave_(event: Event) {
    // If mouse moves from one element to another the 'dragenter'
    // event for the new element comes before the 'dragleave' event for
    // the old one. In this case event.target !== this.lastEnteredTarget_
    // and handler of the 'dragenter' event has already carried of
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

  private async onDrop_(onlyIntoDirectories: boolean, event: DragEvent) {
    if (onlyIntoDirectories && !this.dropTarget_) {
      return;
    }
    const destinationEntry =
        this.destinationEntry_ || this.directoryModel_.getCurrentDirEntry();
    assert(destinationEntry);
    if (getRootType(destinationEntry) === RootType.TRASH &&
        this.canTrashSelection_(
            getRootType(destinationEntry), event.dataTransfer)) {
      event.preventDefault();
      const sourceURLs =
          (event?.dataTransfer?.getData('fs/sources') || '').split('\n');
      const {entries, failureUrls} =
          await convertURLsToEntriesWithAccess(sourceURLs);

      // The list of entries should not be special entries (e.g. Camera, Linux
      // files) and should not already exist in Trash (i.e. you can't trash
      // something that's already trashed).
      const isModifiableAndNotInTrashRoot = (entry: Entry) => {
        return !isNonModifiable(this.volumeManager_, entry) &&
            !isTrashEntry(entry);
      };
      const canTrashEntries = entries && entries.length > 0 &&
          entries.every(isModifiableAndNotInTrashRoot);
      if (canTrashEntries && (!failureUrls || failureUrls.length === 0)) {
        startIOTask(
            chrome.fileManagerPrivate.IoTaskType.TRASH, entries,
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
        event.dataTransfer, destinationEntry,
        this.selectDropEffect_(
            event, this.getDragAndDropGlobalData_(), destinationEntry));
    this.clearDropTarget_();
  }

  /**
   * Change to the drop target directory.
   */
  private changeToDropTargetDirectory_() {
    // Do custom action.
    if (isTreeItem(this.dropTarget_)) {
      this.dropTarget_.doDropTargetAction();
    }
    if (!this.destinationEntry_) {
      return;
    }
    this.directoryModel_.changeDirectoryEntry(this.destinationEntry_);
  }

  protected isDropTargetAllowed_(destinationEntryURL: string) {
    // Disallow dropping a directory either on itself or on one of its children.
    const {sourceURLs} = this.getDragAndDropGlobalData_() || {sourceURLs: ''};
    const destinationURLWithTrailingSlash =
        destinationEntryURL + (destinationEntryURL.endsWith('/') ? '' : '/');
    for (const url of sourceURLs.split('\n')) {
      if (url === destinationEntryURL ||
          destinationURLWithTrailingSlash.startsWith(url)) {
        // Note: When this method is called, destinationEntry is always a
        // directory and its URL doesn't have a trailing slash.
        return false;
      }
    }

    // Disallow drop target for disabled destination entries.
    const fileData = getFileData(getStore().getState(), destinationEntryURL);
    if (fileData?.disabled) {
      return false;
    }

    return true;
  }

  /**
   * Sets the drop target.
   */
  private setDropTarget_(
      domElement: Element|null, clipboardData: DataTransfer,
      destinationEntry: DirectoryEntry|FakeEntry) {
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

    if (!this.isDropTargetAllowed_(destinationEntry.toURL())) {
      return;
    }

    this.destinationEntry_ = destinationEntry;

    // Add accept classes if the directory can accept this drop.
    if (this.canPasteOrDrop_(clipboardData, destinationEntry)) {
      domElement.classList.remove('denies');
      domElement.classList.add('accepts');
    }

    // Change directory immediately if it's a fake entry for Crostini.
    if (getRootType(destinationEntry) === RootType.CROSTINI) {
      this.changeToDropTargetDirectory_();
      return;
    }

    // Change to the directory after the drag target hover time out.
    const navigate = this.changeToDropTargetDirectory_.bind(this);
    this.navigateTimer_ = setTimeout(navigate, this.dragTargetHoverTime_());
  }

  /**
   * Return the drag target hover time in milliseconds.
   */
  private dragTargetHoverTime_() {
    return window.IN_TEST ? 500 : 2000;
  }

  /**
   * Handles touch start.
   */
  private onTouchStart_() {
    this.touching_ = true;
  }

  /**
   * Handles touch end.
   */
  private onTouchEnd_() {
    // TODO(fukino): We have to check if event.touches.length be 0 to support
    // multi-touch operations, but event.touches has incorrect value by a bug
    // (crbug.com/373125).
    // After the bug is fixed, we should check event.touches.
    this.touching_ = false;
  }

  /**
   * Clears the drop target.
   */
  private clearDropTarget_() {
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

  protected isDocumentWideEvent_() {
    const element = this.document_.activeElement;
    const tagName = this.document_.activeElement?.nodeName.toLowerCase();

    return !(
        (tagName === 'input' &&
         (element && 'type' in element && element?.type === 'text')) ||
        tagName === 'cr-input');
  }

  private onCutOrCopy_(isMove: boolean, event: DragEvent|ClipboardEvent) {
    if (!this.isDocumentWideEvent_() || !this.canCutOrCopy_(isMove)) {
      return;
    }

    event.preventDefault();

    const clipboardData = getClipboardData(event);
    const effectAllowed = isMove ? 'move' : 'copy';

    // If current focus is on DirectoryTree, write selected item of
    // DirectoryTree to system clipboard.
    if (document.activeElement && isXfTree(document.activeElement)) {
      const focusedItem = getFocusedTreeItem(document.activeElement);
      this.cutOrCopyFromDirectoryTree(
          focusedItem, clipboardData, effectAllowed);
      return;
    }
    if (document.activeElement && isTreeItem(document.activeElement)) {
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
   */
  cutOrCopyFromDirectoryTree(
      focusedItem: XfTreeItem|null, clipboardData: DataTransfer|null,
      effectAllowed: DataTransfer['effectAllowed']) {
    if (focusedItem === null) {
      return;
    }

    const entry = focusedItem && getTreeItemEntry(focusedItem);
    if (!entry) {
      return;
    }

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    if (!volumeInfo) {
      return;
    }

    // When this value is false, we cannot copy between different sources.
    const missingFileContents = volumeInfo.volumeType === VolumeType.DRIVE &&
        this.volumeManager_.getDriveConnectionState().type ===
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE;

    this.appendCutOrCopyInfo_(
        clipboardData, effectAllowed, volumeInfo, [entry], missingFileContents);
  }

  private onBeforeCutOrCopy_(isMove: boolean, event: Event) {
    if (!this.isDocumentWideEvent_()) {
      return;
    }

    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCutOrCopy_(isMove)) {
      event.preventDefault();
    }
  }

  private canCutOrCopy_(isMove: boolean) {
    const command = isMove ? this.cutCommand_ : this.copyCommand_;
    command.canExecuteChange(this.document_.activeElement);
    return !command.disabled;
  }

  canCopyOrDrag() {
    if (!this.selectionHandler_.isAvailable()) {
      return false;
    }
    if (this.selectionHandler_.selection.entries.length <= 0) {
      return false;
    }
    // Trash entries are only allowed to be restored which is analogous to a
    // cut event, so disallow the copy.
    if (this.selectionHandler_.selection.entries.every(isTrashEntry)) {
      return false;
    }
    const entries = this.selectionHandler_.selection.entries;
    for (let i = 0; i < entries.length; i++) {
      if (!entries[i]) {
        continue;
      }
      if (isTeamDriveRoot(entries[i]!)) {
        return false;
      }
      // If selected entries are not in the same directory, we can't copy them
      // by a single operation at this moment.
      if (i > 0 && !isSiblingEntry(entries[0]!, entries[i]!)) {
        return false;
      }
    }
    // Don't allow copy of encrypted files.
    if (this.metadataModel_.getCache(entries, ['contentMimeType'])
            .every(
                (metadata, i) => entries[i] ?
                    isEncrypted(entries[i]!, metadata.contentMimeType) :
                    false)) {
      return false;
    }

    // Check if canCopy is true or undefined, but not false (see
    // https://crbug.com/849999).
    return this.metadataModel_.getCache(entries, ['canCopy'])
        .every(item => item.canCopy !== false);
  }

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
      if (entries[i] && isNonModifiable(this.volumeManager_, entries[i]!)) {
        return false;
      }
    }

    return true;
  }

  private onPaste_(event: DragEvent|ClipboardEvent|
                   PasteWithDestDirectoryEvent) {
    // If the event has destDirectory property, paste files into the directory.
    // This occurs when the command fires from menu item 'Paste into folder'.
    const destination =
        (('destDirectory' in event && event.destDirectory) ||
         this.directoryModel_.getCurrentDirEntry()) as DirectoryEntry;

    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_() ||
        !this.canPasteOrDrop_(getClipboardData(event), destination)) {
      return;
    }
    event.preventDefault();
    this.paste(getClipboardData(event), destination).then(effect => {
      // On cut, we clear the clipboard after the file is pasted/moved so we
      // don't try to move/delete the original file again.
      if (effect === 'move') {
        this.simulateCommand_('cut', (event: Event) => {
          event.preventDefault();
          const clipboardData = getClipboardData(event);
          if (clipboardData) {
            clipboardData.setData('fs/clear', '');
          }
        });
      }
    });
  }

  private onBeforePaste_(event: Event) {
    if (!this.isDocumentWideEvent_()) {
      return;
    }
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    const currentDirEntry = this.directoryModel_.getCurrentDirEntry();
    if (currentDirEntry &&
        this.canPasteOrDrop_(getClipboardData(event), currentDirEntry)) {
      event.preventDefault();
    }
  }

  private canPasteOrDrop_(
      clipboardData: DataTransfer|null,
      destinationEntry: FakeEntry|DirectoryEntry|FilesAppDirEntry|null|
      undefined) {
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
    if (destinationLocationInfo.rootType === RootType.RECENT) {
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
    if (destinationLocationInfo.rootType === RootType.TRASH) {
      return this.canTrashSelection_(
          getRootType(destinationLocationInfo), clipboardData);
    }

    const sourceUrls = (clipboardData.getData('fs/sources') || '').split('\n');
    assert(destinationLocationInfo.volumeInfo);
    if (this.getSourceRootUrl_(
            clipboardData, this.getDragAndDropGlobalData_()) !==
        destinationLocationInfo.volumeInfo.fileSystem.root.toURL()) {
      // Copying between different sources requires all files to be available.
      if (this.isMissingFileContents_(clipboardData)) {
        return false;
      }

      // Moving an encrypted files outside of Google Drive is not supported.
      if (this.isEncrypted_(clipboardData)) {
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
              source => getFileTypeForName(source).type === 'hosted')) {
        return false;
      }
    }

    // If the destination is sub-tree of any of the sources paste isn't allowed.
    const addTrailingSlash = (s: string) => {
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
    const metadata = this.metadataModel_.getCache(
        [destinationEntry as DirectoryEntry], ['canAddChildren']);
    if (metadata[0]?.canAddChildren === false) {
      return false;
    }

    return true;
  }

  /**
   * Execute paste command.
   */
  queryPasteCommandEnabled(destinationEntry: DirectoryEntry|FilesAppDirEntry|
                           null|undefined) {
    if (!this.isDocumentWideEvent_()) {
      return false;
    }

    // HACK(serya): return this.document_.queryCommandEnabled('paste')
    // should be used.
    let result;
    this.simulateCommand_('paste', (event: Event) => {
      result = this.canPasteOrDrop_(getClipboardData(event), destinationEntry);
    });
    return result;
  }

  /**
   * Allows to simulate commands to get access to clipboard.
   */
  private simulateCommand_(command: string, handler: (e: Event) => void) {
    const iframe = this.document_.body.querySelector<HTMLIFrameElement>(
        '#command-dispatcher')!;
    const doc = iframe.contentDocument;
    if (!doc) {
      return;
    }
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  }

  private onFileSelectionChangedThrottled_() {
    // Remove file objects that are no longer in the selection.
    const asyncData: FileAsyncData = {};
    const entries = this.selectionHandler_.selection.entries;
    for (const entry of entries) {
      const entryUrl = entry.toURL();
      if (entryUrl in this.selectedAsyncData_) {
        asyncData[entryUrl] = this.selectedAsyncData_[entryUrl]!;
      }
    }
    this.selectedAsyncData_ = asyncData;

    const fileEntries: FileEntry[] = [];
    for (const entry of entries) {
      if (entry.isFile) {
        fileEntries.push(entry as FileEntry);
      }
      const entryUrl = entry.toURL();
      if (!(entryUrl in asyncData)) {
        asyncData[entryUrl] = {externalFileUrl: '', file: undefined};
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
          const fileEntryURL = fileEntry?.toURL();
          if (!(fileEntryURL && asyncData[fileEntryURL]?.file) && fileEntry) {
            fileEntry.file(file => {
              if (asyncData[fileEntryURL!]) {
                asyncData[fileEntryURL!]!.file = file;
              }
            });
          }
        })(fileEntries[i]);
      }
    }

    this.metadataModel_
        .get(entries, ['alternateUrl', 'externalFileUrl', 'hosted'])
        .then(metadataList => {
          for (let i = 0; i < entries.length; i++) {
            if (!entries[i]) {
              continue;
            }
            const entryUrl = entries[i]!.toURL();
            if (entries[i]!.isFile) {
              if (metadataList[i]?.hosted) {
                asyncData[entryUrl]!.externalFileUrl =
                    metadataList[i]!.alternateUrl;
              } else {
                asyncData[entryUrl]!.externalFileUrl =
                    metadataList[i]!.externalFileUrl;
              }
            }
          }
        });
  }

  private selectDropEffect_(
      event: DragEvent, dragAndDropData: DragAndDropGlobalData|null,
      destinationEntry: FilesAppDirEntry|DirectoryEntry): DropEffectType {
    if (!destinationEntry) {
      return DropEffectType.NONE;
    }
    const destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo) {
      return DropEffectType.NONE;
    }
    if (destinationLocationInfo.volumeInfo &&
        destinationLocationInfo.volumeInfo.error) {
      return DropEffectType.NONE;
    }
    // Recent isn't read-only, but it doesn't support drop.
    if (destinationLocationInfo.rootType === RootType.RECENT) {
      return DropEffectType.NONE;
    }
    if (destinationLocationInfo.isReadOnly) {
      if (destinationLocationInfo.isSpecialSearchRoot) {
        // The location is a fake entry that corresponds to special search.
        return DropEffectType.NONE;
      }
      if (destinationLocationInfo.rootType === RootType.CROSTINI) {
        // The location is a the fake entry for crostini.  Start container.
        return DropEffectType.NONE;
      }
      if (destinationLocationInfo.volumeInfo &&
          destinationLocationInfo.volumeInfo.isReadOnlyRemovableDevice) {
        return DropEffectType.NONE;
      }
      // The disk device is not write-protected but read-only.
      // Currently, the only remaining possibility is that write access to
      // removable drives is restricted by device policy.
      return DropEffectType.NONE;
    }
    // Decryption of CSE files is not currently supported on ChromeOS. However,
    // moving such a file around Google Drive works fine.
    if (dragAndDropData && dragAndDropData.encrypted &&
        destinationLocationInfo.rootType !== RootType.DRIVE) {
      return DropEffectType.NONE;
    }
    const destinationMetadata = this.metadataModel_.getCache(
        [destinationEntry as DirectoryEntry], ['canAddChildren']);
    if (destinationMetadata.length === 1 &&
        destinationMetadata[0]!.canAddChildren === false) {
      // TODO(sashab): Distinguish between copy/move operations and display
      // corresponding warning text here.
      return DropEffectType.NONE;
    }
    // Files can be dragged onto the TrashRootEntry, but they must reside on a
    // volume that is trashable.
    if (destinationLocationInfo.rootType === RootType.TRASH) {
      const effect =
          (this.canTrashSelection_(
              getRootType(destinationLocationInfo), event.dataTransfer!)) ?
          DropEffectType.MOVE :
          DropEffectType.NONE;
      return effect;
    }
    if (isDropEffectAllowed(event.dataTransfer!.effectAllowed, 'move')) {
      if (!isDropEffectAllowed(event.dataTransfer!.effectAllowed, 'copy')) {
        return DropEffectType.MOVE;
      }
      // TODO(mtomasz): Use volumeId instead of comparing roots, as soon as
      // volumeId gets unique.
      assert(destinationLocationInfo.volumeInfo);
      if (this.getSourceRootUrl_(event.dataTransfer!, dragAndDropData) ===
              destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
          !event.ctrlKey) {
        return DropEffectType.MOVE;
      }
      if (event.shiftKey) {
        return DropEffectType.MOVE;
      }
    }
    return DropEffectType.COPY;
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
   */
  private canTrashSelection_(
      rootType: RootType|null, clipboardData: DataTransfer|null) {
    if (!rootType) {
      return false;
    }
    if (rootType !== RootType.TRASH) {
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
        if (isNonModifiable(this.volumeManager_, entry)) {
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
        this.getSourceRootUrl_(clipboardData, this.getDragAndDropGlobalData_());
    return enabledTrashURLs.some(
        volumeURL => sourceRootURL.startsWith(volumeURL));
  }

  /**
   * Blinks the selection. Used to give feedback when copying or cutting the
   * selection.
   */
  private blinkSelection_() {
    const selection = this.selectionHandler_.selection;
    if (!selection || selection.totalCount === 0) {
      return;
    }

    const listItems: ListItem[] = [];
    for (let i = 0; i < selection.entries.length; i++) {
      const selectedIndex = selection.indexes[i];
      const listItem =
          this.listContainer_.currentList.getListItemByIndex(selectedIndex!);
      if (listItem) {
        listItem.classList.add('blink');
        listItems.push(listItem);
      }
    }

    setTimeout(() => {
      for (let i = 0; i < listItems.length; i++) {
        listItems[i]!.classList.remove('blink');
      }
    }, 100);
  }
}

/**
 * Container for defining a copy/move operation.
 */
export class PastePlan {
  failureUrls: string[] = [];

  constructor(
      public sourceURLs: string[], public sourceEntries: Entry[],
      public destinationEntry: DirectoryEntry|FilesAppDirEntry,
      private metadataModel_: MetadataModel, public isMove: boolean) {}

  /**
   * Resolves sourceEntries from sourceURLs if needed and returns them.
   */
  async resolveEntries() {
    if (!this.sourceEntries.length) {
      const result = await convertURLsToEntriesWithAccess(this.sourceURLs);
      this.sourceEntries = result.entries;
      this.failureUrls = result.failureUrls;
    }
    return this.sourceEntries;
  }

  /**
   * Obtains whether the planned operation requires user's confirmation, as well
   * as its type.
   */
  getConfirmationType() {
    assert(this.sourceEntries[0]);

    // Confirmation type for local drive.
    const sourceEntryCache =
        this.metadataModel_.getCache([this.sourceEntries[0]], ['shared']);
    const destinationEntryCache = this.metadataModel_.getCache(
        [this.destinationEntry as DirectoryEntry], ['shared']);

    // The shared property tells us whether an entry is shared on Drive, and is
    // potentially undefined.
    const isSharedSource = sourceEntryCache[0]?.shared === true;
    const isSharedDestination = destinationEntryCache[0]?.shared === true;

    // See crbug.com/731583#c20.
    if (!isSharedSource && isSharedDestination) {
      return this.isMove ? TransferConfirmationType.MOVE_TO_SHARED_DRIVE :
                           TransferConfirmationType.COPY_TO_SHARED_DRIVE;
    }

    // Confirmation type for team drives.
    const source = {
      isTeamDrive: isSharedDriveEntry(this.sourceEntries[0]),
      teamDriveName: getTeamDriveName(this.sourceEntries[0]),
    };
    const destination = {
      isTeamDrive: isSharedDriveEntry(this.destinationEntry),
      teamDriveName: getTeamDriveName(this.destinationEntry),
    };
    if (this.isMove) {
      if (source.isTeamDrive) {
        if (destination.isTeamDrive) {
          if (source.teamDriveName === destination.teamDriveName) {
            return TransferConfirmationType.NONE;
          } else {
            return TransferConfirmationType.MOVE_BETWEEN_SHARED_DRIVES;
          }
        } else {
          return TransferConfirmationType.MOVE_FROM_SHARED_DRIVE_TO_OTHER;
        }
      } else if (destination.isTeamDrive) {
        return TransferConfirmationType.MOVE_FROM_OTHER_TO_SHARED_DRIVE;
      }
      return TransferConfirmationType.NONE;
    } else {
      if (!destination.isTeamDrive) {
        return TransferConfirmationType.NONE;
      }
      // Copying to Shared Drive.
      if (!(source.isTeamDrive &&
            source.teamDriveName === destination.teamDriveName)) {
        // This is not a copy within the same Shared Drive.
        return TransferConfirmationType.COPY_FROM_OTHER_TO_SHARED_DRIVE;
      }
      return TransferConfirmationType.NONE;
    }
  }

  /**
   * Composes a confirmation message for the given type.
   */
  getConfirmationMessages(confirmationType: TransferConfirmationType) {
    assert(this.sourceEntries.length !== 0);
    const sourceName = getTeamDriveName(this.sourceEntries[0]!);
    const destinationName = getTeamDriveName(this.destinationEntry);
    switch (confirmationType) {
      case TransferConfirmationType.COPY_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_COPY_TO_SHARED_DRIVE',
            this.destinationEntry.fullPath.split('/').pop())];
      case TransferConfirmationType.MOVE_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_MOVE_TO_SHARED_DRIVE',
            this.destinationEntry.fullPath.split('/').pop())];
      case TransferConfirmationType.MOVE_BETWEEN_SHARED_DRIVES:
        return [
          strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName),
          strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName),
        ];
      // TODO(yamaguchi): notify ownership transfer if the two Shared Drives
      // belong to different domains.
      case TransferConfirmationType.MOVE_FROM_SHARED_DRIVE_TO_OTHER:
        return [
          strf('DRIVE_CONFIRM_TD_MEMBERS_LOSE_ACCESS', sourceName),
          // TODO(yamaguchi): Warn if the operation moves at least one
          // directory to My Drive, as it's no undoable.
        ];
      case TransferConfirmationType.MOVE_FROM_OTHER_TO_SHARED_DRIVE:
        return [strf('DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS', destinationName)];
      case TransferConfirmationType.COPY_FROM_OTHER_TO_SHARED_DRIVE:
        return [strf(
            'DRIVE_CONFIRM_TD_MEMBERS_GAIN_ACCESS_TO_COPY', destinationName)];
    }
    assertNotReached('Invalid confirmation type: ' + confirmationType);
    return [];
  }
}

/**
 * Converts list of urls to list of Entries with granting R/W permissions to
 * them, which is essential when pasting files from a different profile.
 */
const convertURLsToEntriesWithAccess = async (urls: string[]) => {
  await grantAccess(urls);
  return convertURLsToEntries(urls);
};


/**
 * Checks if the specified set of allowed effects contains the given effect.
 * See: http://www.w3.org/TR/html5/editing.html#the-datatransfer-interface
 */
const isDropEffectAllowed =
    (effectAllowed: DataTransfer['effectAllowed']|null, dropEffect: string) => {
      return effectAllowed === 'all' ||
          effectAllowed?.toLowerCase().indexOf(dropEffect) !== -1;
    };
