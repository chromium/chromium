// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImageLoaderClient} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/image_loader_client.js';
import {LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from 'chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/load_image_request.js';
import {assert} from 'chrome://resources/ash/common/assert.js';

import {DialogType, isModal} from '../../common/js/dialog_type.js';
import {FileType} from '../../common/js/file_type.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {CommandHandlerDeps} from '../../externs/command_handler_deps.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {FilesQuickView} from '../elements/files_quick_view.js';

import {constants} from './constants.js';
import {CommandHandler} from './file_manager_commands.js';
import {FileSelectionHandler} from './file_selection.js';
import {FileTasks, parseActionId} from './file_tasks.js';
import {MetadataItem} from './metadata/metadata_item.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {MetadataBoxController} from './metadata_box_controller.js';
import {QuickViewModel} from './quick_view_model.js';
import {QuickViewUma} from './quick_view_uma.js';
import {TaskController} from './task_controller.js';
import {ThumbnailLoader} from './thumbnail_loader.js';
import {FileListSelectionModel} from './ui/file_list_selection_model.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';
import {ListContainer} from './ui/list_container.js';
import {MultiMenuButton} from './ui/multi_menu_button.js';

/**
 * Controller for QuickView.
 */
export class QuickViewController {
  /**
   * This should be initialized with |init_| method.
   *
   * @param {!CommandHandlerDeps} fileManager
   * @param {!MetadataModel} metadataModel
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!ListContainer} listContainer
   * @param {!MultiMenuButton} selectionMenuButton
   * @param {!QuickViewModel} quickViewModel
   * @param {!TaskController} taskController
   * @param {!FileListSelectionModel} fileListSelectionModel
   * @param {!QuickViewUma} quickViewUma
   * @param {!MetadataBoxController} metadataBoxController
   * @param {DialogType} dialogType
   * @param {!VolumeManager} volumeManager
   * @param {!HTMLElement} dialogDom
   */
  constructor(
      fileManager, metadataModel, selectionHandler, listContainer,
      selectionMenuButton, quickViewModel, taskController,
      fileListSelectionModel, quickViewUma, metadataBoxController, dialogType,
      volumeManager, dialogDom) {
    /** @private {!CommandHandlerDeps} */
    this.fileManager_ = fileManager;

    /** @private {?FilesQuickView} */
    this.quickView_ = null;

    /** @private @const {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    /** @private @const {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const{!QuickViewModel} */
    this.quickViewModel_ = quickViewModel;

    /** @private @const {!QuickViewUma} */
    this.quickViewUma_ = quickViewUma;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!TaskController} */
    this.taskController_ = taskController;

    /** @private @const {!FileListSelectionModel} */
    this.fileListSelectionModel_ = fileListSelectionModel;

    /** @private @const {!MetadataBoxController} */
    this.metadataBoxController_ = metadataBoxController;

    /** @private @const {DialogType} */
    this.dialogType_ = dialogType;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /**
     * Delete confirm dialog.
     * @private {?FilesConfirmDialog}
     */
    this.deleteConfirmDialog_ = null;

    /**
     * Current selection of selectionHandler.
     * @private {!Array<!FileEntry>}
     */
    this.entries_ = [];

    /**
     * The tasks for the current entry shown in quick view.
     * @private {?FileTasks}
     */
    this.tasks_ = null;

    /**
     * The current selection index of this.entries_.
     * @private {number}
     */
    this.currentSelection_ = 0;

    /**
     * Stores whether we are in check-select mode or not.
     * @private {boolean}
     */
    this.checkSelectMode_ = false;

    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE,
        this.onFileSelectionChanged_.bind(this));
    this.listContainer_.element.addEventListener(
        'keydown', this.onKeyDownToOpen_.bind(this));
    dialogDom.addEventListener('command', event => {
      // Selection menu command can be triggered with focus outside of file list
      // or button e.g.: from the directory tree.
      if (event.command.id === 'get-info') {
        event.stopPropagation();
        this.display_(QuickViewUma.WayToOpen.SELECTION_MENU);
      }
    });
    this.listContainer_.element.addEventListener('command', event => {
      if (event.command.id === 'get-info') {
        event.stopPropagation();
        this.display_(QuickViewUma.WayToOpen.CONTEXT_MENU);
      }
    });
    selectionMenuButton.addEventListener('command', event => {
      if (event.command.id === 'get-info') {
        event.stopPropagation();
        this.display_(QuickViewUma.WayToOpen.SELECTION_MENU);
      }
    });
  }

  /**
   * Initialize the controller with quick view which will be lazily loaded.
   * @param {!FilesQuickView} quickView
   * @private
   */
  init_(quickView) {
    this.quickView_ = quickView;
    this.quickView_.isModal = isModal(this.dialogType_);

    this.quickView_.setAttribute('files-ng', '');

    this.metadataBoxController_.init(quickView);

    document.body.addEventListener(
        'keydown', this.onQuickViewKeyDown_.bind(this));
    this.quickView_.addEventListener('close', () => {
      this.listContainer_.focus();
    });

    this.quickView_.onOpenInNewButtonTap =
        this.onOpenInNewButtonTap_.bind(this);

    this.quickView_.onDeleteButtonTap = this.onDeleteButtonTap_.bind(this);

    const toolTipElements =
        this.quickView_.$$('#toolbar').querySelectorAll('[has-tooltip]');
    this.quickView_.$$('files-tooltip').addTargets(toolTipElements);
  }

  /**
   * Create quick view element.
   * @return {!FilesQuickView}
   * @private
   */
  createQuickView_() {
    return /** @type {!FilesQuickView} */ (
        document.querySelector('#quick-view'));
  }

  /**
   * Handles open-in-new button tap.
   *
   * @param {!Event} event A button click event.
   * @private
   */
  onOpenInNewButtonTap_(event) {
    this.tasks_ && this.tasks_.executeDefault();
    this.quickView_.close();
  }

  /**
   * Handles delete button tap.
   *
   * @param {!Event} event A button click event.
   * @private
   */
  onDeleteButtonTap_(event) {
    this.deleteSelectedEntry_();
  }

  /**
   * Handles key event on listContainer if it's relevant to quick view.
   *
   * @param {!Event} event A keyboard event.
   * @private
   */
  onKeyDownToOpen_(event) {
    if (event.key === ' ') {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (this.entries_.length > 0) {
        this.display_(QuickViewUma.WayToOpen.SPACE_KEY);
      }
    }
  }

  /**
   * Handles key event on quick view.
   *
   * @param {!Event} event A keyboard event.
   * @private
   */
  onQuickViewKeyDown_(event) {
    if (this.quickView_.isOpened()) {
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
   *
   * @param {boolean} down True if user pressed down arrow, false if up.
   * @private
   */
  changeSingleSelectModeSelection_(down = false) {
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
   *
   * @param {boolean} down True if user pressed down arrow, false if up.
   * @private
   */
  changeCheckSelectModeSelection_(down = false) {
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

    this.onFileSelectionChanged_(null);
  }

  /**
   * Delete the currently selected entry in quick view.
   *
   * @private
   */
  deleteSelectedEntry_() {
    const entry = this.entries_[this.currentSelection_];

    // Create a delete confirm dialog if needed.
    if (!this.deleteConfirmDialog_) {
      const dialogElement = document.createElement('dialog');
      this.quickView_.shadowRoot.appendChild(dialogElement);
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
    CommandHandler.getCommand('delete').deleteEntries(
        [entry], this.fileManager_, /*permanentlyDelete=*/ false,
        this.deleteConfirmDialog_);
  }

  /**
   * Returns true if the entry can be deleted.
   * @param {Entry} entry
   * @return {!Promise<boolean>}
   * @private
   */
  canDeleteEntry_(entry) {
    const deleteCommand = CommandHandler.getCommand('delete');
    return Promise.resolve(
        deleteCommand.canDeleteEntries([entry], this.fileManager_));
  }

  /**
   * Display quick view.
   *
   * @param {QuickViewUma.WayToOpen} wayToOpen The open quick view trigger.
   * @private
   */
  async display_(wayToOpen) {
    // On opening Quick View, always reset the current selection index.
    this.currentSelection_ = 0;

    this.checkSelectMode_ = this.fileListSelectionModel_.getCheckSelectMode();

    await this.updateQuickView_();
    if (!this.quickView_.isOpened()) {
      this.quickView_.open();
      this.quickViewUma_.onOpened(this.entries_[0], wayToOpen);
    }
  }

  /**
   * Update quick view on file selection change.
   *
   * @param {?Event} event an Event whose target is FileSelectionHandler.
   * @private
   */
  onFileSelectionChanged_(event) {
    if (event) {
      this.entries_ = event.target.selection.entries;

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

    if (this.quickView_ && this.quickView_.isOpened()) {
      assert(this.entries_.length > 0);
      const entry = this.entries_[this.currentSelection_];

      if (!util.isSameEntry(entry, this.quickViewModel_.getSelectedEntry())) {
        this.updateQuickView_();
      }
    }
  }

  /**
   * Update quick view using current entries.
   *
   * @return {!Promise} Promise fulfilled after quick view is updated.
   * @private
   */
  async updateQuickView_() {
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

    assert(this.entries_.length > 0);
    const entry = this.entries_[this.currentSelection_];
    this.quickViewModel_.setSelectedEntry(entry);
    this.tasks_ = null;

    requestIdleCallback(() => {
      this.quickViewUma_.onEntryChanged(entry);
    });

    try {
      const values = await Promise.all([
        this.metadataModel_.get([entry], ['thumbnailUrl', 'modificationTime']),
        this.taskController_.getEntryFileTasks(entry),
        this.canDeleteEntry_(entry),
      ]);

      const items = /**@type{Array<MetadataItem>}*/ (values[0]);
      const tasks = /**@type{!FileTasks}*/ (values[1]);
      const canDelete = values[2];
      return this.onMetadataLoaded_(entry, items, tasks, canDelete);
    } catch (error) {
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
   *
   * @param {!FileEntry} entry
   * @param {Array<MetadataItem>} items
   * @param {!FileTasks} fileTasks
   * @param {boolean} canDelete
   * @return {!Promise}
   * @private
   */
  async onMetadataLoaded_(entry, items, fileTasks, canDelete) {
    const tasks = fileTasks.getAnnotatedTasks();

    const params =
        await this.getQuickViewParameters_(entry, items, tasks, canDelete);
    if (this.quickViewModel_.getSelectedEntry() != entry) {
      return;  // Bail: there's no point drawing a stale selection.
    }

    const emptySourceContent = {
      data: null,
      dataType: '',
    };

    this.quickView_.setProperties({
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

  /**
   * @param {!FileEntry} entry
   * @param {Array<MetadataItem>} items
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   * @param {boolean} canDelete
   * @return {!Promise<!QuickViewParams>}
   *
   * @private
   */
  async getQuickViewParameters_(entry, items, tasks, canDelete) {
    const typeInfo = FileType.getType(entry);
    const type = typeInfo.type;
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const label = util.getEntryLabel(locationInfo, entry);
    const entryIsOnDrive = locationInfo && locationInfo.isDriveBased;
    const thumbnailUrl = items.length ? items[0].thumbnailUrl : undefined;
    const modificationTime =
        items.length ? items[0].modificationTime : undefined;

    /** @type {!QuickViewParams} */
    const params = {
      type: type,
      subtype: typeInfo.subtype,
      filePath: label,
      hasTask: tasks.length > 0,
      canDelete: canDelete,
    };

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    let localFile = volumeInfo &&
        QuickViewController.LOCAL_VOLUME_TYPES_.indexOf(
            assert(volumeInfo.volumeType)) >= 0;

    // Treat certain types on Drive as if they were local (try auto-play etc).
    if (entryIsOnDrive && (type === 'audio' || type === 'video')) {
      localFile = true;
    }

    if (!localFile) {
      // Drive files: Try to fetch their thumbnail or fallback to read the whole
      // file.
      if (thumbnailUrl) {
        const result =
            await this.loadThumbnailFromDrive_(thumbnailUrl, modificationTime);
        if (result.status === LoadImageResponseStatus.SUCCESS) {
          if (params.type == 'video') {
            params.videoPoster = {
              data: result.data,
              dataType: 'url',
            };
          } else if (params.type == 'image') {
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
    }

    if (type === 'raw') {
      // RAW files: fetch their ImageLoader thumbnail.
      try {
        const result = await this.loadRawFileThumbnailFromImageLoader_(entry);
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
      const file = await new Promise((resolve, reject) => {
        entry.file(resolve, reject);
      });

      switch (type) {
        case 'image':
          if (QuickViewController.UNSUPPORTED_IMAGE_SUBTYPES_.indexOf(
                  typeInfo.subtype) === -1) {
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
          if (item.contentThumbnailUrl) {
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
   *
   * @param {string} url Thumbnail url
   * @param {Date|undefined} modificationTime File's modification time.
   * @return {!Promise<!LoadImageResponse>}
   * @private
   */
  async loadThumbnailFromDrive_(url, modificationTime) {
    const client = ImageLoaderClient.getInstance();
    const request = LoadImageRequest.createForUrl(url);
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
   *
   * @param {!Entry} entry The RAW file entry.
   * @return {!Promise<!LoadImageResponse>}
   * @private
   */
  loadRawFileThumbnailFromImageLoader_(entry) {
    return new Promise((resolve, reject) => {
      entry.file(function requestFileThumbnail(file) {
        const request = LoadImageRequest.createForUrl(entry.toURL());
        request.maxWidth = ThumbnailLoader.THUMBNAIL_MAX_WIDTH;
        request.maxHeight = ThumbnailLoader.THUMBNAIL_MAX_HEIGHT;
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
 *
 * @private @const {!Array<!VolumeManagerCommon.VolumeType>}
 */
QuickViewController.LOCAL_VOLUME_TYPES_ = [
  VolumeManagerCommon.VolumeType.ARCHIVE,
  VolumeManagerCommon.VolumeType.DOWNLOADS,
  VolumeManagerCommon.VolumeType.REMOVABLE,
  VolumeManagerCommon.VolumeType.ANDROID_FILES,
  VolumeManagerCommon.VolumeType.CROSTINI,
  VolumeManagerCommon.VolumeType.GUEST_OS,
  VolumeManagerCommon.VolumeType.MEDIA_VIEW,
  VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
  VolumeManagerCommon.VolumeType.SMB,
];

/**
 * List of unsupported image subtypes excluded from being displayed in
 * QuickView. An "unsupported type" message is shown instead.
 * @private @const {!Array<string>}
 */
QuickViewController.UNSUPPORTED_IMAGE_SUBTYPES_ = [
  'TIFF',  // crbug.com/624109
];
