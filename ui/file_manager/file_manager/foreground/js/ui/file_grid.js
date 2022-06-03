// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {dispatchSimpleEvent} from 'chrome://resources/js/cr.m.js';
import {Grid, GridSelectionController} from 'chrome://resources/js/cr/ui/grid.m.js';
import {List} from 'chrome://resources/js/cr/ui/list.m.js';
import {ListItem} from 'chrome://resources/js/cr/ui/list_item.m.js';
import {ListSelectionModel} from 'chrome://resources/js/cr/ui/list_selection_model.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';

import {AsyncUtil} from '../../../common/js/async_util.js';
import {FileType} from '../../../common/js/file_type.js';
import {importer} from '../../../common/js/importer_common.js';
import {str, util} from '../../../common/js/util.js';
import {importerHistoryInterfaces} from '../../../externs/background/import_history.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {ListThumbnailLoader} from '../list_thumbnail_loader.js';
import {MetadataModel} from '../metadata/metadata_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {filelist} from './file_table_list.js';
import {FileTapHandler} from './file_tap_handler.js';

/**
 * FileGrid constructor.
 *
 * Represents grid for the Grid View in the File Manager.
 */

export class FileGrid extends Grid {
  constructor() {
    super();

    /** @private {number} */
    this.paddingTop_ = 0;

    /** @private {number} */
    this.paddingStart_ = 0;

    /** @private {number} */
    this.beginIndex_ = 0;

    /** @private {number} */
    this.endIndex_ = 0;

    /**
     * Inherited from Grid <- List
     * @private {?Element}
     * */
    this.beforeFiller_ = null;

    /**
     * Inherited from Grid <- List
     * @private {?Element}
     * */
    this.afterFiller_ = null;

    /**
     * Reflects the visibility of import status in the UI.  Assumption: import
     * status is only enabled in import-eligible locations.  See
     * ImportController#onDirectoryChanged.  For this reason, the code in this
     * class checks if import status is visible, and if so, assumes that all the
     * files are in an import-eligible location.
     * TODO(kenobi): Clean this up once import status is queryable from
     * metadata.
     *
     * @private {boolean}
     */
    this.importStatusVisible_ = true;

    /** @private {?MetadataModel} */
    this.metadataModel_ = null;

    /** @private {?ListThumbnailLoader} */
    this.listThumbnailLoader_ = null;

    /** @private {?VolumeManager} */
    this.volumeManager_ = null;

    /** @private {?importerHistoryInterfaces.HistoryLoader} */
    this.historyLoader_ = null;

    /** @private {?AsyncUtil.RateLimiter} */
    this.relayoutRateLimiter_ = null;

    /** @private {?function(!Event)} */
    this.onThumbnailLoadedBound_ = null;

    /** @private {?ObjectPropertyDescriptor|undefined} */
    this.dataModelDescriptor_ = null;

    /** @public {?A11yAnnounce} */
    this.a11y = null;

    throw new Error('Use FileGrid.decorate');
  }

  get dataModel() {
    if (!this.dataModelDescriptor_) {
      // We get the property descriptor for dataModel from List, because
      // Grid doesn't have its own descriptor.
      this.dataModelDescriptor_ =
          Object.getOwnPropertyDescriptor(List.prototype, 'dataModel');
    }
    return this.dataModelDescriptor_.get.call(this);
  }

  set dataModel(model) {
    // The setter for dataModel is overridden to remove/add the 'splice'
    // listener for the current data model.
    if (this.dataModel) {
      this.dataModel.removeEventListener('splice', this.onSplice_.bind(this));
    }
    this.dataModelDescriptor_.set.call(this, model);
    if (this.dataModel) {
      this.dataModel.addEventListener('splice', this.onSplice_.bind(this));
      this.classList.toggle('image-dominant', this.dataModel.isImageDominant());
    }
  }

  /**
   * Decorates an HTML element to be a FileGrid.
   * @param {!Element} element The grid to decorate.
   * @param {!MetadataModel} metadataModel File system metadata.
   * @param {!VolumeManager} volumeManager Volume manager instance.
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   * @param {!A11yAnnounce} a11y
   */
  static decorate(element, metadataModel, volumeManager, historyLoader, a11y) {
    if (Grid.decorate) {
      Grid.decorate(element);
    }
    const self = /** @type {!FileGrid} */ (element);
    self.__proto__ = FileGrid.prototype;
    self.setAttribute('aria-multiselectable', true);
    self.setAttribute('aria-describedby', 'more-actions-info');
    self.metadataModel_ = metadataModel;
    self.volumeManager_ = volumeManager;
    self.historyLoader_ = historyLoader;
    self.a11y = a11y;

    // Force the list's ending spacer to be tall enough to allow overscroll.
    const endSpacer = self.querySelector('.spacer:last-child');
    if (endSpacer) {
      endSpacer.classList.add('signals-overscroll');
    }

    self.listThumbnailLoader_ = null;
    self.beginIndex_ = 0;
    self.endIndex_ = 0;
    self.importStatusVisible_ = true;
    self.onThumbnailLoadedBound_ = self.onThumbnailLoaded_.bind(self);

    self.itemConstructor = function(entry) {
      let item = self.ownerDocument.createElement('li');
      item.__proto__ = FileGrid.Item.prototype;
      item = /** @type {!FileGrid.Item} */ (item);
      self.decorateThumbnail_(item, /** @type {!Entry} */ (entry));
      return item;
    };

    self.relayoutRateLimiter_ =
        new AsyncUtil.RateLimiter(self.relayoutImmediately_.bind(self));

    const style = window.getComputedStyle(self);
    self.paddingStart_ =
        parseFloat(isRTL() ? style.paddingRight : style.paddingLeft);
    self.paddingTop_ = parseFloat(style.paddingTop);
  }

  /**
   * @param {number} index Index of the list item.
   * @return {string}
   */
  getItemLabel(index) {
    if (index === -1) {
      return '';
    }

    /** @type {Entry|FilesAppEntry} */
    const entry = this.dataModel.item(index);
    if (!entry) {
      return '';
    }

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    return util.getEntryLabel(locationInfo, entry);
  }

  /**
   * Sets list thumbnail loader.
   * @param {ListThumbnailLoader} listThumbnailLoader A list thumbnail loader.
   */
  setListThumbnailLoader(listThumbnailLoader) {
    if (this.listThumbnailLoader_) {
      this.listThumbnailLoader_.removeEventListener(
          'thumbnailLoaded', this.onThumbnailLoadedBound_);
    }

    this.listThumbnailLoader_ = listThumbnailLoader;

    if (this.listThumbnailLoader_) {
      this.listThumbnailLoader_.addEventListener(
          'thumbnailLoaded', this.onThumbnailLoadedBound_);
      this.listThumbnailLoader_.setHighPriorityRange(
          this.beginIndex_, this.endIndex_);
    }
  }

  /**
   * Returns the element containing the thumbnail of a certain list item as
   * background image.
   * @param {number} index The index of the item containing the desired
   *     thumbnail.
   * @return {?Element} The element containing the thumbnail, or null, if an
   *     error occurred.
   */
  getThumbnail(index) {
    const listItem = this.getListItemByIndex(index);
    if (!listItem) {
      return null;
    }
    const container = listItem.querySelector('.img-container');
    if (!container) {
      return null;
    }
    return container.querySelector('.thumbnail');
  }

  /**
   * Handles thumbnail loaded event.
   * @param {!Event} event An event.
   * @private
   */
  onThumbnailLoaded_(event) {
    const listItem = this.getListItemByIndex(event.index);
    const entry = listItem && this.dataModel.item(listItem.listIndex);
    if (entry) {
      const box = listItem.querySelector('.img-container');
      if (box) {
        const mimeType =
            this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
                .contentMimeType;
        if (!event.dataUrl) {
          FileGrid.clearThumbnailImage_(assertInstanceof(box, HTMLDivElement));
          this.setGenericThumbnail_(
              assertInstanceof(box, HTMLDivElement), entry, mimeType);
        } else {
          FileGrid.setThumbnailImage_(
              assertInstanceof(box, HTMLDivElement), entry,
              assert(event.dataUrl), assert(event.width), assert(event.height),
              mimeType);
        }
      }
      listItem.classList.toggle('thumbnail-loaded', !!event.dataUrl);
    }
  }

  /**
   * @override
   */
  mergeItems(beginIndex, endIndex) {
    List.prototype.mergeItems.call(this, beginIndex, endIndex);

    const afterFiller = this.afterFiller_;
    const columns = this.columns;
    let previousTitle = '';

    for (let item = this.beforeFiller_.nextSibling; item !== afterFiller;) {
      const next = item.nextSibling;
      if (isSpacer(item)) {
        // Spacer found on a place it mustn't be.
        this.removeChild(item);
        item = next;
        continue;
      }
      const index = item.listIndex;
      const nextIndex = index + 1;

      const entry = this.dataModel.item(index);
      if (entry) {
        if (entry.isDirectory && previousTitle !== 'dir') {
          // For first Directory we add a title div before the element.
          const title = document.createElement('div');
          title.innerText = str('GRID_VIEW_FOLDERS_TITLE');
          title.classList.add('grid-title', 'folders');
          this.insertBefore(title, item);
          previousTitle = 'dir';
        } else if (!entry.isDirectory && previousTitle !== 'file') {
          // For first File we add a title div before the element.
          const title = document.createElement('div');
          title.innerText = str('GRID_VIEW_FILES_TITLE');
          title.classList.add('grid-title', 'files');
          this.insertBefore(title, item);
          previousTitle = 'file';
        }
      }

      // Invisible pinned item could be outside of the
      // [beginIndex, endIndex). Ignore it.
      if (index >= beginIndex && nextIndex < endIndex &&
          (nextIndex < this.dataModel.getFolderCount() ?
               nextIndex % columns === 0 :
               (nextIndex - this.dataModel.getFolderCount()) % columns === 0)) {
        const isFolderSpacer = nextIndex === this.dataModel.getFolderCount();
        if (isSpacer(next)) {
          // Leave the spacer on its place.
          next.classList.toggle('folder-spacer', isFolderSpacer);
          item = next.nextSibling;
        } else {
          // Insert spacer.
          const spacer = this.ownerDocument.createElement('div');
          spacer.className = 'spacer';
          spacer.classList.toggle('folder-spacer', isFolderSpacer);
          this.insertBefore(spacer, next);
          item = next;
        }
      } else {
        item = next;
      }
    }

    function isSpacer(child) {
      return child.classList.contains('spacer') &&
          child !== afterFiller;  // Must not be removed.
    }

    // Make sure that grid item's selected attribute is updated just after the
    // mergeItems operation is done. This prevents shadow of selected grid items
    // from being animated unintentionally by redraw.
    for (let i = beginIndex; i < endIndex; i++) {
      const item = this.getListItemByIndex(i);
      if (!item) {
        continue;
      }
      const isSelected = this.selectionModel.getIndexSelected(i);
      if (item.selected !== isSelected) {
        item.selected = isSelected;
      }
    }

    // Keep these values to set range when a new list thumbnail loader is set.
    this.beginIndex_ = beginIndex;
    this.endIndex_ = endIndex;
    if (this.listThumbnailLoader_ !== null) {
      this.listThumbnailLoader_.setHighPriorityRange(beginIndex, endIndex);
    }
  }

  /**
   * @override
   */
  getItemTop(index) {
    if (index < this.dataModel.getFolderCount()) {
      return Math.floor(index / this.columns) * this.getFolderItemHeight_();
    }

    const folderRows = this.getFolderRowCount();
    const indexInFiles = index - this.dataModel.getFolderCount();
    return folderRows * this.getFolderItemHeight_() +
        (folderRows > 0 ? this.getSeparatorHeight_() : 0) +
        Math.floor(indexInFiles / this.columns) * this.getFileItemHeight_();
  }

  /**
   * @override
   */
  getItemRow(index) {
    if (index < this.dataModel.getFolderCount()) {
      return Math.floor(index / this.columns);
    }

    const folderRows = this.getFolderRowCount();
    const indexInFiles = index - this.dataModel.getFolderCount();
    return folderRows + Math.floor(indexInFiles / this.columns);
  }

  /**
   * Returns the column of an item which has given index.
   * @param {number} index The item index.
   */
  getItemColumn(index) {
    if (index < this.dataModel.getFolderCount()) {
      return index % this.columns;
    }

    const indexInFiles = index - this.dataModel.getFolderCount();
    return indexInFiles % this.columns;
  }

  /**
   * Return the item index which is placed at the given position.
   * If there is no item in the given position, returns -1.
   * @param {number} row The row index.
   * @param {number} column The column index.
   */
  getItemIndex(row, column) {
    if (row < 0 || column < 0 || column >= this.columns) {
      return -1;
    }
    const folderCount = this.dataModel.getFolderCount();
    const folderRows = this.getFolderRowCount();
    let index;
    if (row < folderRows) {
      index = row * this.columns + column;
      return index < folderCount ? index : -1;
    }
    index = folderCount + (row - folderRows) * this.columns + column;
    return index < this.dataModel.length ? index : -1;
  }

  /**
   * @override
   */
  getFirstItemInRow(row) {
    const folderRows = this.getFolderRowCount();
    if (row < folderRows) {
      return row * this.columns;
    }

    return this.dataModel.getFolderCount() + (row - folderRows) * this.columns;
  }

  /**
   * @override
   */
  scrollIndexIntoView(index) {
    const dataModel = this.dataModel;
    if (!dataModel || index < 0 || index >= dataModel.length) {
      return;
    }

    const itemHeight = index < this.dataModel.getFolderCount() ?
        this.getFolderItemHeight_() :
        this.getFileItemHeight_();
    const scrollTop = this.scrollTop;
    const top = this.getItemTop(index);
    const clientHeight = this.clientHeight;

    const computedStyle = window.getComputedStyle(this);
    const paddingY = parseInt(computedStyle.paddingTop, 10) +
        parseInt(computedStyle.paddingBottom, 10);
    const availableHeight = clientHeight - paddingY;

    const self = this;
    // Function to adjust the tops of viewport and row.
    const scrollToAdjustTop = () => {
      self.scrollTop = top;
    };
    // Function to adjust the bottoms of viewport and row.
    const scrollToAdjustBottom = () => {
      self.scrollTop = top + itemHeight - availableHeight;
    };

    // Check if the entire of given indexed row can be shown in the viewport.
    if (itemHeight <= availableHeight) {
      if (top < scrollTop) {
        scrollToAdjustTop();
      } else if (scrollTop + availableHeight < top + itemHeight) {
        scrollToAdjustBottom();
      }
    } else {
      if (scrollTop < top) {
        scrollToAdjustTop();
      } else if (top + itemHeight < scrollTop + availableHeight) {
        scrollToAdjustBottom();
      }
    }
  }

  /**
   * @override
   */
  getItemsInViewPort(scrollTop, clientHeight) {
    const beginRow = this.getRowForListOffset_(scrollTop);
    const endRow = this.getRowForListOffset_(scrollTop + clientHeight - 1) + 1;
    const beginIndex = this.getFirstItemInRow(beginRow);
    const endIndex =
        Math.min(this.getFirstItemInRow(endRow), this.dataModel.length);
    const result = {
      first: beginIndex,
      length: endIndex - beginIndex,
      last: endIndex - 1
    };
    return result;
  }

  /**
   * @override
   */
  getAfterFillerHeight(lastIndex) {
    const folderRows = this.getFolderRowCount();
    const fileRows = this.getFileRowCount();
    const row = this.getItemRow(lastIndex - 1);
    if (row < folderRows) {
      let fillerHeight = (folderRows - 1 - row) * this.getFolderItemHeight_() +
          fileRows * this.getFileItemHeight_();
      if (fileRows > 0) {
        fillerHeight += this.getSeparatorHeight_();
      }
      return fillerHeight;
    }
    const rowInFiles = row - folderRows;
    return (fileRows - 1 - rowInFiles) * this.getFileItemHeight_();
  }

  /**
   * Returns the number of rows in folders section.
   * @return {number}
   */
  getFolderRowCount() {
    return Math.ceil(this.dataModel.getFolderCount() / this.columns);
  }

  /**
   * Returns the number of rows in files section.
   * @return {number}
   */
  getFileRowCount() {
    return Math.ceil(this.dataModel.getFileCount() / this.columns);
  }

  /**
   * Returns the height of folder items in grid view.
   * @return {number} The height of folder items.
   */
  getFolderItemHeight_() {
    return 44;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns the height of file items in grid view.
   * @return {number} The height of file items.
   */
  getFileItemHeight_() {
    return 184;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns the width of grid items.
   * @return {number}
   */
  getItemWidth_() {
    return 184;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns the margin top of grid items.
   * @return {number};
   */
  getItemMarginTop_() {
    return 4;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns the margin left of grid items.
   * @return {number}
   */
  getItemMarginLeft_() {
    return 4;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns the height of the separator which separates folders and files.
   * @return {number} The height of the separator.
   */
  getSeparatorHeight_() {
    return 5;  // TODO(fukino): Read from DOM and cache it.
  }

  /**
   * Returns index of a row which contains the given y-position(offset).
   * @param {number} offset The offset from the top of grid.
   * @return {number} Row index corresponding to the given offset.
   * @private
   */
  getRowForListOffset_(offset) {
    const innerOffset = Math.max(0, offset - this.paddingTop_);
    const folderRows = this.getFolderRowCount();
    if (innerOffset < folderRows * this.getFolderItemHeight_()) {
      return Math.floor(innerOffset / this.getFolderItemHeight_());
    }

    let offsetInFiles = innerOffset - folderRows * this.getFolderItemHeight_();
    if (folderRows > 0) {
      offsetInFiles = Math.max(0, offsetInFiles - this.getSeparatorHeight_());
    }
    return folderRows + Math.floor(offsetInFiles / this.getFileItemHeight_());
  }

  /**
   * @override
   */
  createSelectionController(sm) {
    return new FileGridSelectionController(assert(sm), this);
  }

  /**
   * Updates items to reflect metadata changes.
   * @param {string} type Type of metadata changed.
   * @param {Array<Entry>} entries Entries whose metadata changed.
   */
  updateListItemsMetadata(type, entries) {
    const urls = util.entriesToURLs(entries);
    const boxes = /** @type {!NodeList<!HTMLElement>} */ (
        this.querySelectorAll('.img-container'));
    for (let i = 0; i < boxes.length; i++) {
      const box = boxes[i];
      const listItem = this.getListItemAncestor(box);
      const entry = listItem && this.dataModel.item(listItem.listIndex);
      if (!entry || urls.indexOf(entry.toURL()) === -1) {
        continue;
      }

      this.decorateThumbnailBox_(assert(listItem), entry);
      this.updateSharedStatus_(assert(listItem), entry);
    }
  }

  /**
   * Redraws the UI. Skips multiple consecutive calls.
   */
  relayout() {
    this.relayoutRateLimiter_.run();
  }

  /**
   * Redraws the UI immediately.
   * @private
   */
  relayoutImmediately_() {
    this.startBatchUpdates();
    this.columns = 0;
    this.redraw();
    this.endBatchUpdates();
    dispatchSimpleEvent(this, 'relayout');
  }

  /**
   * Decorates thumbnail.
   * @param {ListItem} li List item.
   * @param {!Entry} entry Entry to render a thumbnail for.
   * @private
   */
  decorateThumbnail_(li, entry) {
    li.className = 'thumbnail-item';
    if (entry) {
      filelist.decorateListItem(li, entry, assert(this.metadataModel_));
    }

    const frame = li.ownerDocument.createElement('div');
    frame.className = 'thumbnail-frame';
    li.appendChild(frame);

    const box = li.ownerDocument.createElement('div');
    box.classList.add('img-container', 'no-thumbnail');
    frame.appendChild(box);
    if (entry) {
      this.decorateThumbnailBox_(assertInstanceof(li, HTMLLIElement), entry);
    }

    const badge = li.ownerDocument.createElement('div');
    badge.className = 'badge';
    frame.appendChild(badge);

    const bottom = li.ownerDocument.createElement('div');
    bottom.className = 'thumbnail-bottom';
    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            .contentMimeType;
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const detailIcon = filelist.renderFileTypeIcon(
        li.ownerDocument, entry, locationInfo, mimeType);

    // For FilesNg we add the checkmark in the same location.
    const checkmark = li.ownerDocument.createElement('div');
    checkmark.className = 'detail-checkmark';
    detailIcon.appendChild(checkmark);
    bottom.appendChild(detailIcon);
    bottom.appendChild(
        filelist.renderFileNameLabel(li.ownerDocument, entry, locationInfo));
    frame.appendChild(bottom);
    li.setAttribute('file-name', util.getEntryLabel(locationInfo, entry));

    this.updateSharedStatus_(li, entry);
  }

  /**
   * Decorates the box containing a centered thumbnail image.
   *
   * @param {!HTMLLIElement} li List item which contains the box to be
   *     decorated.
   * @param {Entry} entry Entry which thumbnail is generating for.
   * @private
   */
  decorateThumbnailBox_(li, entry) {
    const box =
        assertInstanceof(li.querySelector('.img-container'), HTMLDivElement);
    if (this.importStatusVisible_ && importer.isEligibleType(entry)) {
      this.historyLoader_.getHistory().then(FileGrid.applyHistoryBadges_.bind(
          null,
          /** @type {!FileEntry} */ (entry), box));
    }

    if (entry.isDirectory) {
      this.setGenericThumbnail_(box, entry);
      return;
    }

    // Set thumbnail if it's already in cache, and the thumbnail data is not
    // empty.
    const thumbnailData = this.listThumbnailLoader_ ?
        this.listThumbnailLoader_.getThumbnailFromCache(entry) :
        null;
    const mimeType =
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            .contentMimeType;
    if (thumbnailData && thumbnailData.dataUrl) {
      FileGrid.setThumbnailImage_(
          box, entry, thumbnailData.dataUrl, (thumbnailData.width || 0),
          (thumbnailData.height || 0), mimeType);
      li.classList.toggle('thumbnail-loaded', true);
    } else {
      this.setGenericThumbnail_(box, entry, mimeType);
      li.classList.toggle('thumbnail-loaded', false);
    }
  }

  /**
   * Added 'shared' class to icon and placeholder of a folder item.
   * @param {!HTMLLIElement} li The grid item.
   * @param {!Entry} entry File entry for the grid item.
   * @private
   */
  updateSharedStatus_(li, entry) {
    if (!entry.isDirectory) {
      return;
    }

    const shared =
        !!this.metadataModel_.getCache([entry], ['shared'])[0].shared;
    const box = li.querySelector('.img-container');
    if (box) {
      box.classList.toggle('shared', shared);
    }
    const icon = li.querySelector('.detail-icon');
    if (icon) {
      icon.classList.toggle('shared', shared);
    }
  }

  /**
   * Sets the visibility of the cloud import status column.
   * @param {boolean} visible
   */
  setImportStatusVisible(visible) {
    this.importStatusVisible_ = visible;
  }

  /**
   * Handles the splice event of the data model to change the view based on
   * whether image files is dominant or not in the directory.
   * @private
   */
  onSplice_() {
    // When adjusting search parameters, |dataModel| is transiently empty.
    // Updating whether image-dominant is active at these times can cause
    // spurious changes. Avoid this problem by not updating whether
    // image-dominant is active when |dataModel| is empty.
    if (this.dataModel.getFileCount() === 0 &&
        this.dataModel.getFolderCount() === 0) {
      return;
    }
    this.classList.toggle('image-dominant', this.dataModel.isImageDominant());
  }

  /**
   * Sets thumbnail image to the box.
   * @param {!HTMLDivElement} box A div element to hold thumbnails.
   * @param {!Entry} entry An entry of the thumbnail.
   * @param {string} dataUrl Data url of thumbnail.
   * @param {number} width Width of thumbnail.
   * @param {number} height Height of thumbnail.
   * @param {string=} opt_mimeType Optional mime type for the image.
   * @private
   */
  static setThumbnailImage_(box, entry, dataUrl, width, height, opt_mimeType) {
    const thumbnail = box.ownerDocument.createElement('div');
    thumbnail.classList.add('thumbnail');
    box.classList.toggle('no-thumbnail', false);

    // If the image is JPEG or the thumbnail is larger than the grid size,
    // resize it to cover the thumbnail box.
    const type = FileType.getType(entry, opt_mimeType);
    if ((type.type === 'image' && type.subtype === 'JPEG') ||
        width > FileGrid.GridSize || height > FileGrid.GridSize) {
      thumbnail.style.backgroundSize = 'cover';
    }

    thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';

    const oldThumbnails = box.querySelectorAll('.thumbnail');
    for (let i = 0; i < oldThumbnails.length; i++) {
      box.removeChild(oldThumbnails[i]);
    }

    box.appendChild(thumbnail);
  }


  /**
   * Clears thumbnail image from the box.
   * @param {!HTMLDivElement} box A div element to hold thumbnails.
   * @private
   */
  static clearThumbnailImage_(box) {
    const oldThumbnails = box.querySelectorAll('.thumbnail');
    for (let i = 0; i < oldThumbnails.length; i++) {
      box.removeChild(oldThumbnails[i]);
    }
    box.classList.toggle('no-thumbnail', true);
    return;
  }

  /**
   * Sets a generic thumbnail on the box.
   * @param {!HTMLDivElement} box A div element to hold thumbnails.
   * @param {!Entry} entry An entry of the thumbnail.
   * @param {string=} opt_mimeType Optional mime type for the file.
   * @private
   */
  setGenericThumbnail_(box, entry, opt_mimeType) {
    if (entry.isDirectory) {
      box.setAttribute('generic-thumbnail', 'folder');
    } else {
      box.classList.toggle('no-thumbnail', true);
      const locationInfo = this.volumeManager_.getLocationInfo(entry);
      const icon = FileType.getIcon(entry, opt_mimeType, locationInfo.rootType);
      box.setAttribute('generic-thumbnail', icon);
    }
  }

  /**
   * Applies cloud import history badges as appropriate for the Entry.
   *
   * @param {!FileEntry} entry
   * @param {Element} box Box to decorate.
   * @param {!importerHistoryInterfaces.ImportHistory} history
   *
   * @private
   */
  static applyHistoryBadges_(entry, box, history) {
    history.wasImported(entry, importer.Destination.GOOGLE_DRIVE)
        .then(imported => {
          if (imported) {
            // TODO(smckay): update badges when history changes
            // "box" is currently the sibling of the elemement
            // we want to style. So rather than employing
            // a possibly-fragile sibling selector we just
            // plop the imported class on the parent of both.
            box.parentElement.classList.add('imported');
          } else {
            history.wasCopied(entry, importer.Destination.GOOGLE_DRIVE)
                .then(copied => {
                  if (copied) {
                    // TODO(smckay): update badges when history changes
                    // "box" is currently the sibling of the elemement
                    // we want to style. So rather than employing
                    // a possibly-fragile sibling selector we just
                    // plop the imported class on the parent of both.
                    box.parentElement.classList.add('copied');
                  }
                });
          }
        });
  }

  /**
   * Returns whether the drag event is inside a file entry in the list (and not
   * the background padding area).
   * @param {MouseEvent} event Drag start event.
   * @return {boolean} True if the mouse is over an element in the list, False
   *     if
   *                   it is in the background.
   */
  hasDragHitElement(event) {
    const pos = DragSelector.getScrolledPosition(this, event);
    return this.getHitElements(pos.x, pos.y).length !== 0;
  }

  /**
   * Obtains if the drag selection should be start or not by referring the mouse
   * event.
   * @param {MouseEvent} event Drag start event.
   * @return {boolean} True if the mouse is hit to the background of the list.
   */
  shouldStartDragSelection(event) {
    // Start dragging area if the drag starts outside of the contents of the
    // grid.
    return !this.hasDragHitElement(event);
  }

  /**
   * Returns the index of row corresponding to the given y position.
   *
   * If the reverse is false, this returns index of the first row in which
   * bottom of grid items is greater than or equal to y. Otherwise, this returns
   * index of the last row in which top of grid items is less than or equal to
   * y.
   * @param {number} y
   * @param {boolean} reverse
   * @return {number}
   * @private
   */
  getHitRowIndex_(y, reverse) {
    const folderRows = this.getFolderRowCount();
    const folderHeight = this.getFolderItemHeight_();
    const fileHeight = this.getFileItemHeight_();

    if (y < folderHeight * folderRows) {
      const shift = reverse ? -this.getItemMarginTop_() : 0;
      return Math.floor((y + shift) / folderHeight);
    }
    let yInFiles = y - folderHeight * folderRows;
    if (folderRows > 0) {
      yInFiles = Math.max(0, yInFiles - this.getSeparatorHeight_());
    }
    const shift = reverse ? -this.getItemMarginTop_() : 0;
    return folderRows + Math.floor((yInFiles + shift) / fileHeight);
  }

  /**
   * Returns the index of column corresponding to the given x position.
   *
   * If the reverse is false, this returns index of the first column in which
   * left of grid items is greater than or equal to x. Otherwise, this returns
   * index of the last column in which right of grid items is less than or equal
   * to x.
   * @param {number} x
   * @param {boolean} reverse
   * @return {number}
   * @private
   */
  getHitColumnIndex_(x, reverse) {
    const itemWidth = this.getItemWidth_();
    const shift = reverse ? -this.getItemMarginLeft_() : 0;
    return Math.floor((x + shift) / itemWidth);
  }

  /**
   * Obtains the index list of elements that are hit by the point or the
   * rectangle.
   *
   * We should match its argument interface with FileList.getHitElements.
   *
   * @param {number} x X coordinate value.
   * @param {number} y Y coordinate value.
   * @param {number=} opt_width Width of the coordinate.
   * @param {number=} opt_height Height of the coordinate.
   * @return {Array<number>} Index list of hit elements.
   */
  getHitElements(x, y, opt_width, opt_height) {
    const currentSelection = [];
    const startXWithPadding = isRTL() ? this.clientWidth - (x + opt_width) : x;
    const startX = Math.max(0, startXWithPadding - this.paddingStart_);
    const endX = startX + (opt_width ? opt_width - 1 : 0);
    const top = Math.max(0, y - this.paddingTop_);
    const bottom = top + (opt_height ? opt_height - 1 : 0);

    const firstRow = this.getHitRowIndex_(top, false);
    const lastRow = this.getHitRowIndex_(bottom, true);
    const firstColumn = this.getHitColumnIndex_(startX, false);
    const lastColumn = this.getHitColumnIndex_(endX, true);

    for (let row = firstRow; row <= lastRow; row++) {
      for (let col = firstColumn; col <= lastColumn; col++) {
        const index = this.getItemIndex(row, col);
        if (0 <= index && index < this.dataModel.length) {
          currentSelection.push(index);
        }
      }
    }
    return currentSelection;
  }
}

/**
 * Grid size.
 * @const {number}
 */
FileGrid.GridSize = 180;  // px

FileGrid.Item = class extends ListItem {
  constructor() {
    super();
    throw new Error('Use FileGrid.Item.decorate');
  }

  /**
   * @return {string} Label of the item.
   */
  get label() {
    return this.querySelector('filename-label').textContent;
  }

  set label(newLabel) {
    // no-op setter. List calls this setter but Files app doesn't need it.
  }

  /**
   * @override
   */
  decorate() {
    super.decorate();
    // Override the default role 'listitem' to 'option' to match the parent's
    // role (listbox).
    this.setAttribute('role', 'option');
    const nameId = this.id + '-entry-name';
    this.querySelector('.entry-name').setAttribute('id', nameId);
    this.querySelector('.img-container')
        .setAttribute('aria-labelledby', nameId);
    this.setAttribute('aria-labelledby', nameId);
  }
};

/**
 * Selection controller for the file grid.
 */
export class FileGridSelectionController extends GridSelectionController {
  /**
   * @param {!ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {!Grid} grid The grid to interact with.
   */
  constructor(selectionModel, grid) {
    super(selectionModel, grid);

    /**
     * @type {!FileTapHandler}
     * @const
     */
    this.tapHandler_ = new FileTapHandler();
  }

  /** @override */
  handlePointerDownUp(e, index) {
    filelist.handlePointerDownUp.call(this, e, index);
  }

  /** @override */
  handleTouchEvents(e, index) {
    if (this.tapHandler_.handleTouchEvents(
            assert(e), index, filelist.handleTap.bind(this))) {
      filelist.focusParentList(e);
    }
  }

  /** @override */
  handleKeyDown(e) {
    filelist.handleKeyDown.call(this, e);
  }

  /** @return {!FileGrid} */
  get filesView() {
    return /** @type {!FileGrid} */ (this.grid_);
  }

  /** @override */
  getIndexBelow(index) {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexAfter(index);
    }
    if (index === this.getLastIndex()) {
      return -1;
    }

    const grid = /** @type {!FileGrid} */ (this.grid_);
    const row = grid.getItemRow(index);
    const col = grid.getItemColumn(index);
    const nextIndex = grid.getItemIndex(row + 1, col);
    if (nextIndex === -1) {
      return row + 1 < grid.getFolderRowCount() ?
          grid.dataModel.getFolderCount() - 1 :
          grid.dataModel.length - 1;
    }
    return nextIndex;
  }

  /** @override */
  getIndexAbove(index) {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexBefore(index);
    }
    if (index === 0) {
      return -1;
    }

    const grid = /** @type {!FileGrid} */ (this.grid_);
    const row = grid.getItemRow(index);
    if (row - 1 < 0) {
      return 0;
    }
    const col = grid.getItemColumn(index);
    const nextIndex = grid.getItemIndex(row - 1, col);
    if (nextIndex === -1) {
      return row - 1 < grid.getFolderRowCount() ?
          grid.dataModel.getFolderCount() - 1 :
          grid.dataModel.length - 1;
    }
    return nextIndex;
  }
}
