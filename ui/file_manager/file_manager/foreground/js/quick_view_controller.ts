// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import type {LoadImageResponse} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {createForUrl, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isModal} from '../../common/js/dialog_type.js';
import {isSameEntry} from '../../common/js/entry_utils.js';
import {parseActionId} from '../../common/js/file_tasks.js';
import {getType} from '../../common/js/file_type.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {getEntryLabel, str} from '../../common/js/translations.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';
import type {DialogType} from '../../state/state.js';
import type {FilesQuickView} from '../elements/files_quick_view.js';
import type {FilesTooltip} from '../elements/files_tooltip.js';

import {CommandHandler, type CommandHandlerDeps} from './command_handler.js';
import type {FileSelectionHandler} from './file_selection.js';
import {EventType} from './file_selection.js';
import type {FileTasks} from './file_tasks.js';
import type {MetadataItem} from './metadata/metadata_item.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {MetadataBoxController} from './metadata_box_controller.js';
import type {QuickViewModel} from './quick_view_model.js';
import type {QuickViewUma} from './quick_view_uma.js';
import {WayToOpen} from './quick_view_uma.js';
import type {TaskController} from './task_controller.js';
import {THUMBNAIL_MAX_HEIGHT, THUMBNAIL_MAX_WIDTH} from './thumbnail_loader.js';
import type {CommandEvent} from './ui/command.js';
import type {FileListSelectionModel, FileListSingleSelectionModel} from './ui/file_list_selection_model.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';
import type {ListContainer} from './ui/list_container.js';
import type {MultiMenuButton} from './ui/multi_menu_button.js';

/**
 * Controller for QuickView.
 */
export class QuickViewController {
  private quickView_: FilesQuickView|null = null;

  /**
   * Delete confirm dialog.
   */
  private deleteConfirmDialog_: FilesConfirmDialog|null = null;

  /**
   * Current selection of selectionHandler.
   */
  private entries_: Array<Entry|FilesAppEntry> = [];

  /**
   * The tasks for the current entry shown in quick view.
   */
  private tasks_: FileTasks|null = null;

  /**
   * The current selection index of this.entries_.
   */
  private currentSelection_ = 0;

  /**
   * Stores whether we are in check-select mode or not.
   */
  private checkSelectMode_ = false;

  constructor(
      private fileManager_: CommandHandlerDeps,
      private metadataModel_: MetadataModel,
      private selectionHandler_: FileSelectionHandler,
      private listContainer_: ListContainer,
      selectionMenuButton: MultiMenuButton,
      private quickViewModel_: QuickViewModel,
      private taskController_: TaskController,
      private fileListSelectionModel_: FileListSelectionModel|
      FileListSingleSelectionModel,
      private quickViewUma_: QuickViewUma,
      private metadataBoxController_: MetadataBoxController,
      private dialogType_: DialogType, private volumeManager_: VolumeManager,
      dialogDom: HTMLElement) {
    this.selectionHandler_.addEventListener(
        EventType.CHANGE,
        this.onFileSelectionChanged_.bind(this) as EventListener);
    this.listContainer_.element.addEventListener(
        'keydown', this.onKeyDownToOpen_.bind(this));

    // Selection menu command can be triggered with focus outside of file list
    // or button e.g.: from the directory tree.
    dialogDom.addEventListener(
        'command', this.onCommad_.bind(this, WayToOpen.SELECTION_MENU));
    this.listContainer_.element.addEventListener(
        'command', this.onCommad_.bind(this, WayToOpen.CONTEXT_MENU));
    selectionMenuButton.addEventListener(
        'command', this.onCommad_.bind(this, WayToOpen.SELECTION_MENU));
  }

  private onCommad_(wayToOpen: WayToOpen, event: CommandEvent) {
    if (event.detail.command.id === 'get-info') {
      event.stopPropagation();
      this.display_(wayToOpen);
    }
  }

  /**
   * Initialize the controller with quick view which will be lazily loaded.
   */
  private init_(quickView: FilesQuickView) {
    this.quickView_ = quickView;
    this.quickView_.isModal = isModal(this.dialogType_);

    this.quickView_.setAttribute('files-ng', '');

    this.metadataBoxController_.init(quickView);

    document.body.addEventListener(
        'keydown', this.onQuickViewKeyDown_.bind(this));

    // Prevent selected file from being copied when quick view is open and
    // instead allow any selected "General info" text to be copied.
    document.body.addEventListener('copy', event => {
      if (this.quickView_!.isOpened()) {
        // Stop 'copy' event propagation to FileTransferController and allow
        // default copy event behaviour.
        event.stopPropagation();
      }
    });

    this.quickView_.addEventListener('close', () => {
      this.listContainer_.focus();
    });

    this.quickView_.onOpenInNewButtonClick =
        this.onOpenInNewButtonClick_.bind(this);

    this.quickView_.onDeleteButtonClick = this.onDeleteButtonClick_.bind(this);

    const toolTipElements =
        this.quickView_.shadowRoot!.querySelector<HTMLDivElement>('#toolbar')!
            .querySelectorAll('[has-tooltip]');
    this.quickView_.shadowRoot!.querySelector<FilesTooltip>('files-tooltip')!
        .addTargets(toolTipElements);
  }

  /**
   * Create quick view element.
   */
  private createQuickView_() {
    return document.querySelector<FilesQuickView>('#quick-view')!;
  }

  /**
   * Handles open-in-new button tap.
   */
  private onOpenInNewButtonClick_() {
    this.tasks_ && this.tasks_.executeDefault();
    this.quickView_!.close();
  }

  /**
   * Handles delete button tap.
   */
  private onDeleteButtonClick_() {
    this.deleteSelectedEntry_();
  }

  /**
   * Handles key event on listContainer if it's relevant to quick view.
   */
  private onKeyDownToOpen_(event: KeyboardEvent) {
    if (event.key === ' ') {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (this.entries_.length > 0) {
        this.display_(WayToOpen.SPACE_KEY);
      }
    }
  }

  /**
   * Handles key event on quick view.
   */
  private onQuickViewKeyDown_(event: KeyboardEvent) {
    if (this.quickView_ && this.quickView_.isOpened()) {
      switch (event.key) {
        case ' ':
        case 'Escape':
          event.preventDefault();
          // Prevent the open dialog from closing.
          event.stopImmediatePropagation();
          this.quickView_.close();
          break;
        case 'ArrowRight':
        case 'ArrowDown':
          if (this.fileListSelectionModel_.getCheckSelectMode()) {
            this.changeCheckSelectModeSelection_(true);
          } else {
            this.changeSingleSelectModeSelection_(true);
          }
          break;
        case 'ArrowLeft':
        case 'ArrowUp':
          if (this.fileListSelectionModel_.getCheckSelectMode()) {
            this.changeCheckSelectModeSelection_();
          } else {
            this.changeSingleSelectModeSelection_();
          }
          break;
        case 'Delete':
          this.deleteSelectedEntry_();
          break;
      }
    }
  }

  /**
   * Changes the currently selected entry when in single-select mode.  Sets
   * the models |selectedIndex| to indirectly trigger onFileSelectionChanged_
   * and populate |this.entries_|.
   */
  private changeSingleSelectModeSelection_(down = false) {
    let index;

    if (down) {
      index = this.fileListSelectionModel_.selectedIndex + 1;
      if (index >= this.fileListSelectionModel_.length) {
        index = 0;
      }
    } else {
      index = this.fileListSelectionModel_.selectedIndex - 1;
      if (index < 0) {
        index = this.fileListSelectionModel_.length - 1;
      }
    }

    this.fileListSelectionModel_.selectedIndex = index;
  }

  /**
   * Changes the currently selected entry when in multi-select mode (file
   * list calls this "check-select" mode).
   */
  private changeCheckSelectModeSelection_(down = false) {
    if (down) {
      this.currentSelection_ = this.currentSelection_ + 1;
      if (this.currentSelection_ >=
          this.fileListSelectionModel_.selectedIndexes.length) {
        this.currentSelection_ = 0;
      }
    } else {
      this.currentSelection_ = this.currentSelection_ - 1;
      if (this.currentSelection_ < 0) {
        this.currentSelection_ =
            this.fileListSelectionModel_.selectedIndexes.length - 1;
      }
    }

    this.onFileSelectionChanged_();
  }

  /**
   * Delete the currently selected entry in quick view.
   */
  private deleteSelectedEntry_() {
    const entry = this.entries_[this.currentSelection_];

    // Create a delete confirm dialog if needed.
    if (!this.deleteConfirmDialog_) {
      const dialogElement = document.createElement('dialog');
      this.quickView_!.shadowRoot!.appendChild(dialogElement);
      dialogElement.id = 'delete-confirm-dialog';

      this.deleteConfirmDialog_ = new FilesConfirmDialog(dialogElement);
      this.deleteConfirmDialog_.setOkLabel(str('DELETE_BUTTON_LABEL'));
      this.deleteConfirmDialog_.focusCancelButton = true;

      dialogElement.addEventListener('keydown', event => {
        event.stopPropagation();
      });

      this.deleteConfirmDialog_.showModalElement = () => {
        dialogElement.showModal();
      };

      this.deleteConfirmDialog_.doneCallback = () => {
        dialogElement.close();
      };
    }

    this.checkSelectMode_ = this.fileListSelectionModel_.getCheckSelectMode();

    // Delete the entry if the entry can be deleted.
    const deleteCommand = CommandHandler.getCommand('delete');
    deleteCommand.deleteEntries(
        [entry!], this.fileManager_, /*permanentlyDelete=*/ false,
        this.deleteConfirmDialog_);
  }

  /**
   * Returns true if the entry can be deleted.
   */
  private async canDeleteEntry_(entry: Entry|FilesAppEntry) {
    const deleteCommand = CommandHandler.getCommand('delete');
    return deleteCommand.canDeleteEntries([entry], this.fileManager_);
  }

  /**
   * Display quick view.
   */
  private async display_(wayToOpen: WayToOpen) {
    // On opening Quick View, always reset the current selection index.
    this.currentSelection_ = 0;

    this.checkSelectMode_ = this.fileListSelectionModel_.getCheckSelectMode();

    await this.updateQuickView_();
    if (!this.quickView_!.isOpened()) {
      this.quickView_!.open();
      if (this.entries_[0]) {
        this.quickViewUma_.onOpened(this.entries_[0], wayToOpen);
      }
    }
  }

  /**
   * Update quick view on file selection change.
   */
  private onFileSelectionChanged_(event?: Event&
                                  {target: FileSelectionHandler}) {
    if (event) {
      this.entries_ = event.target?.selection.entries;

      if (!this.entries_ || !this.entries_.length) {
        if (this.quickView_ && this.quickView_.isOpened()) {
          this.quickView_.close();
        }
        return;
      }

      if (this.currentSelection_ >= this.entries_.length) {
        this.currentSelection_ = this.entries_.length - 1;
      } else if (this.currentSelection_ < 0) {
        this.currentSelection_ = 0;
      }

      const checkSelectModeExited = this.checkSelectMode_ !==
          this.fileListSelectionModel_.getCheckSelectMode();
      if (checkSelectModeExited) {
        if (this.quickView_ && this.quickView_.isOpened()) {
          this.quickView_.close();
          return;
        }
      }
    }

    if (this.quickView_ && this.quickView_.isOpened() &&
        this.entries_[this.currentSelection_]) {
      const entry = this.entries_[this.currentSelection_]!;
      if (!isSameEntry(entry, this.quickViewModel_.getSelectedEntry()!)) {
        this.updateQuickView_();
      }
    }
  }

  /**
   * Update quick view using current entries.
   */
  private async updateQuickView_(): Promise<void> {
    if (!this.quickView_) {
      try {
        const quickView = this.createQuickView_();
        this.init_(quickView);
        return this.updateQuickView_();
      } catch (error) {
        console.warn(error);
        return;
      }
    }

    assert(this.entries_.length >= this.currentSelection_);
    const entry = this.entries_[this.currentSelection_]!;
    this.quickViewModel_.setSelectedEntry(entry);
    this.tasks_ = null;

    requestIdleCallback(() => {
      this.quickViewUma_.onEntryChanged(entry);
    });

    try {
      const values = await Promise.all([
        this.metadataModel_.get(
            [entry], ['thumbnailUrl', 'modificationTime', 'contentMimeType']),
        this.taskController_.getEntryFileTasks(entry),
        this.canDeleteEntry_(entry),
      ]);

      const items = values[0];
      const tasks = values[1];
      const canDelete = values[2];
      return this.onMetadataLoaded_(entry, items, tasks, canDelete);
    } catch (error: any) {
      if (error) {
        console.warn(error.stack || error);
      }
    }
  }

  /**
   * Update quick view for |entry| from its loaded metadata and tasks.
   *
   * Note: fast-typing users can change the active selection while the |entry|
   * metadata and tasks were being async fetched. Bail out in that case.
   */
  private async onMetadataLoaded_(
      entry: Entry|FilesAppEntry, items: MetadataItem[], fileTasks: FileTasks,
      canDelete: boolean) {
    const tasks = fileTasks.getAnnotatedTasks();

    const params =
        await this.getQuickViewParameters_(entry, items, tasks, canDelete);
    if (this.quickViewModel_.getSelectedEntry() !== entry) {
      return;  // Bail: there's no point drawing a stale selection.
    }

    const emptySourceContent = {
      data: null,
      dataType: '',
    };

    this.quickView_!.setProperties({
      type: params.type || '',
      subtype: params.subtype || '',
      filePath: params.filePath || '',
      hasTask: params.hasTask || false,
      canDelete: params.canDelete || false,
      sourceContent: params.sourceContent || emptySourceContent,
      videoPoster: params.videoPoster || emptySourceContent,
      audioArtwork: params.audioArtwork || emptySourceContent,
      autoplay: params.autoplay || false,
      browsable: params.browsable || false,
    });

    if (params.hasTask) {
      this.tasks_ = fileTasks;
    }
  }

  private async getQuickViewParameters_(
      entry: FileEntry|Entry|FilesAppEntry, items: MetadataItem[],
      tasks: chrome.fileManagerPrivate.FileTask[],
      canDelete: boolean): Promise<Partial<QuickViewParams>> {
    const firstItem = items[0];
    const typeInfo = getType(entry, firstItem?.contentMimeType);
    const type = typeInfo.type;
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const label = getEntryLabel(locationInfo, entry);
    const entryIsOnDrive = locationInfo && locationInfo.isDriveBased;
    const thumbnailUrl = firstItem ? firstItem.thumbnailUrl : undefined;
    const modificationTime = firstItem ? firstItem.modificationTime : undefined;

    const params: Partial<QuickViewParams> = {
      type: type,
      subtype: typeInfo.subtype,
      filePath: label,
      hasTask: tasks.length > 0,
      canDelete: canDelete,
    };

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    let localFile =
        volumeInfo && LOCAL_VOLUME_TYPES_.indexOf(volumeInfo.volumeType) >= 0;

    // Treat certain types on Drive as if they were local (try auto-play etc).
    if (entryIsOnDrive && (type === 'audio' || type === 'video') &&
        !typeInfo.encrypted) {
      localFile = true;
    }

    if (!localFile) {
      // Drive files: Try to fetch their thumbnail or fallback to read the whole
      // file.
      if (thumbnailUrl) {
        const result =
            await this.loadThumbnailFromDrive_(thumbnailUrl, modificationTime);
        if (result.status === LoadImageResponseStatus.SUCCESS) {
          if (params.type === 'video') {
            params.videoPoster = {
              data: result.data,
              dataType: 'url',
            };
          } else if (params.type === 'image') {
            params.sourceContent = {
              data: result.data,
              dataType: 'url',
            };
          } else {
            params.type = 'image';
            params.sourceContent = {
              data: result.data,
              dataType: 'url',
            };
          }
          return params;
        } else {
          console.warn(`Failed to fetch thumbnail: ${result.status}`);
        }
      }
      if (typeInfo.encrypted) {
        params.type = 'encrypted';
        return params;
      }
    }

    if (type === 'raw') {
      // RAW files: fetch their ImageLoader thumbnail.
      try {
        const result =
            await this.loadRawFileThumbnailFromImageLoader_(entry as FileEntry);
        if (result.status === LoadImageResponseStatus.SUCCESS) {
          params.type = 'image';
          params.sourceContent = {
            data: result.data,
            dataType: 'url',
          };
        } else {
          console.warn(`Failed to fetch thumbnail: ${result.status}`);
        }
        return params;
      } catch (error) {
        console.warn(error);
      }
      return params;
    }

    if (type === '.folder') {
      return Promise.resolve(params);
    }

    try {
      const file =
          await new Promise((resolve: (file: File) => void, reject) => {
            if ('file' in entry) {
              entry.file(resolve, reject);
              return;
            }
            reject(new Error('entry not a file type'));
          });

      switch (type) {
        case 'image':
          if (UNSUPPORTED_IMAGE_SUBTYPES_.indexOf(typeInfo.subtype) === -1) {
            params.sourceContent = {
              data: file,
              dataType: 'blob',
            };
          }
          return params;
        case 'video':
          params.sourceContent = {
            data: file,
            dataType: 'blob',
          };
          params.autoplay = true;
          if (thumbnailUrl) {
            params.videoPoster = {
              data: thumbnailUrl,
              dataType: 'url',
            };
          }
          return params;
        case 'audio':
          params.sourceContent = {
            data: file,
            dataType: 'blob',
          };
          params.autoplay = true;
          const itemsContentThumnbnail =
              await this.metadataModel_.get([entry], ['contentThumbnailUrl']);
          const item = itemsContentThumnbnail[0];
          if (item?.contentThumbnailUrl) {
            params.audioArtwork = {
              data: item.contentThumbnailUrl,
              dataType: 'url',
            };
          }
          return params;
        case 'document':
          if (typeInfo.subtype === 'HTML') {
            params.sourceContent = {
              data: file,
              dataType: 'blob',
            };
            return params;
          }
          break;
        case 'text':
          if (typeInfo.subtype === 'TXT') {
            try {
              const text = await file.text();  // Convert file content to utf-8.
              const blob =
                  await new Blob([text], {type: 'text/plain;charset=utf-8'});
              params.sourceContent = {
                data: blob,
                dataType: 'blob',
              };
              params.browsable = true;
            } catch (error) {
              console.warn(error);
            }
            return params;
          }
          break;
      }

      params.browsable = tasks.some(task => {
        return ['view-in-browser', 'view-pdf'].includes(
            parseActionId(task.descriptor.actionId));
      });

      if (params.browsable) {
        params.sourceContent = {
          data: file,
          dataType: 'blob',
        };
      }
    } catch (error) {
      console.warn(error);
    }
    return params;
  }

  /**
   * Loads a thumbnail from Drive.
   */
  private async loadThumbnailFromDrive_(url: string, modificationTime?: Date):
      Promise<LoadImageResponse> {
    const client = ImageLoaderClient.getInstance();
    const request = createForUrl(url);
    request.cache = true;
    request.timestamp =
        modificationTime ? modificationTime.valueOf() : undefined;
    return new Promise(resolve => client.load(request, resolve));
  }

  /**
   * Loads a RAW image thumbnail from ImageLoader. Resolve the file entry first
   * to get its |lastModified| time. ImageLoaderClient uses that to work out if
   * its cached data for |entry| is up-to-date or otherwise call ImageLoader to
   * refresh the cached |entry| data with the most recent data.
   */
  private async loadRawFileThumbnailFromImageLoader_(entry: FileEntry):
      Promise<LoadImageResponse> {
    return new Promise((resolve, reject) => {
      entry.file((file) => {
        const request = createForUrl(entry.toURL());
        request.maxWidth = THUMBNAIL_MAX_WIDTH;
        request.maxHeight = THUMBNAIL_MAX_HEIGHT;
        request.timestamp = file.lastModified;
        request.cache = true;
        request.priority = 0;
        ImageLoaderClient.getInstance().load(request, resolve);
      }, reject);
    });
  }
}

/**
 * List of local volume types.
 *
 * In this context, "local" means that files in that volume are directly
 * accessible from the Chrome browser process by Linux VFS paths. In this
 * regard, media views are NOT local even though files in media views are
 * actually stored in the local disk.
 *
 * Due to access control of WebView, non-local files can not be previewed
 * with Quick View unless thumbnails are provided (which is the case with
 * Drive).
 */
const LOCAL_VOLUME_TYPES_ = [
  VolumeType.ARCHIVE,
  VolumeType.DOWNLOADS,
  VolumeType.REMOVABLE,
  VolumeType.ANDROID_FILES,
  VolumeType.CROSTINI,
  VolumeType.GUEST_OS,
  VolumeType.MEDIA_VIEW,
  VolumeType.DOCUMENTS_PROVIDER,
  VolumeType.SMB,
];

/**
 * List of unsupported image subtypes excluded from being displayed in
 * QuickView. An "unsupported type" message is shown instead.
 */
const UNSUPPORTED_IMAGE_SUBTYPES_ = [
  'TIFF',  // crbug.com/624109
];
