// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';

import {RateLimiter} from '../../../common/js/async_util.js';
import {maybeShowTooltip} from '../../../common/js/dom_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {str, util} from '../../../common/js/util.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {FileListModel, GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME, GroupValue} from '../file_list_model.js';
import {ListThumbnailLoader} from '../list_thumbnail_loader.js';
import {MetadataModel} from '../metadata/metadata_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {filelist} from './file_table_list.js';
import {FileTapHandler} from './file_tap_handler.js';
import {Grid, GridSelectionController} from './grid.js';
import {List} from './list.js';
import {ListItem} from './list_item.js';
import {ListSelectionModel} from './list_selection_model.js';


// Align with CSS .grid-title.group-by-modificationTime.
const MODIFICATION_TIME_GROUP_HEADING_HEIGHT = 57;
// Align with CSS .grid-title.group-by-isDirectory.
const DIRECTORY_GROUP_HEADING_HEIGHT = 40;
// Align with CSS .grid-title ~ .grid-title
const GROUP_MARGIN_TOP = 16;

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

    /** @private {?MetadataModel} */
    this.metadataModel_ = null;

    /** @private {?ListThumbnailLoader} */
    this.listThumbnailLoader_ = null;

    /** @private {?VolumeManager} */
    this.volumeManager_ = null;

    /** @private {?RateLimiter} */
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
      this.dataModel.removeEventListener('sorted', this.onSorted_.bind(this));
    }
    this.dataModelDescriptor_.set.call(this, model);
    if (this.dataModel) {
      this.dataModel.addEventListener('splice', this.onSplice_.bind(this));
      this.classList.toggle('image-dominant', this.dataModel.isImageDominant());
      this.dataModel.addEventListener('sorted', this.onSorted_.bind(this));
    }
  }

  /**
   * Decorates an HTML element to be a FileGrid.
   * @param {!Element} element The grid to decorate.
   * @param {!MetadataModel} metadataModel File system metadata.
   * @param {!VolumeManager} volumeManager Volume manager instance.
   * @param {!A11yAnnounce} a11y
   */
  static decorate(element, metadataModel, volumeManager, a11y) {
    if (Grid.decorate) {
      Grid.decorate(element);
    }
    const self = /** @type {!FileGrid} */ (element);
    self.__proto__ = FileGrid.prototype;
    self.setAttribute('aria-multiselectable', true);
    self.setAttribute('aria-describedby', 'more-actions-info');
    self.metadataModel_ = metadataModel;
    self.volumeManager_ = volumeManager;
    self.a11y = a11y;

    // Force the list's ending spacer to be tall enough to allow overscroll.
    const endSpacer = self.querySelector('.spacer:last-child');
    if (endSpacer) {
      endSpacer.classList.add('signals-overscroll');
    }

    self.listThumbnailLoader_ = null;
    self.beginIndex_ = 0;
    self.endIndex_ = 0;
    self.onThumbnailLoadedBound_ = self.onThumbnailLoaded_.bind(self);

    self.itemConstructor = function(entry) {
      let item = self.ownerDocument.createElement('li');
      item.__proto__ = FileGrid.Item.prototype;
      item = /** @type {!FileGrid.Item} */ (item);
      self.decorateThumbnail_(item, /** @type {!Entry} */ (entry));
      return item;
    };

    self.relayoutRateLimiter_ =
        new RateLimiter(self.relayoutImmediately_.bind(self));

    const style = window.getComputedStyle(self);
    self.paddingStart_ =
        parseFloat(isRTL() ? style.paddingRight : style.paddingLeft);
    self.paddingTop_ = parseFloat(style.paddingTop);

    self.addEventListener(
        'mouseover', self.onMouseOver_.bind(self), {passive: true});

    // Update the item's inline status when it's restored from List's cache.
    self.addEventListener(
        'cachedItemRestored',
        (e) => filelist.updateCacheItemInlineStatus(
            e.detail, self.dataModel, self.metadataModel_));
  }

  onMouseOver_(event) {
    this.maybeShowToolTip(event);
  }

  maybeShowToolTip(event) {
    let target = null;
    for (const el of event.composedPath()) {
      if (el.classList?.contains('thumbnail-item')) {
        target = el;
        break;
      }
    }
    if (!target) {
      return;
    }
    const labelElement = target.querySelector('.filename-label');
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, labelElement.innerText);
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
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot =
        fileListModel ? fileListModel.getGroupBySnapshot() : [];
    const startIndexToGroupLabel = new Map(groupBySnapshot.map(group => {
      return [group.startIndex, group];
    }));

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
      // Check if index i is the start of a new group.
      if (startIndexToGroupLabel.has(i)) {
        // For first item in each group, we add a title div before the element.
        const title = document.createElement('div');
        title.setAttribute('role', 'heading');
        title.innerText = startIndexToGroupLabel.get(i).label;
        title.classList.add(
            'grid-title', `group-by-${fileListModel.groupByField}`);
        this.insertBefore(title, item);
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
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let top = 0;
    let totalItemCount = 0;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex];
      if (index <= group.endIndex) {
        // The index falls into the current group. Calculates how many rows
        // we have in the current group up until this index.
        const indexInCurGroup = index - totalItemCount;
        const rowsInCurGroup = Math.floor(indexInCurGroup / this.columns);
        top +=
            (rowsInCurGroup > 0 ? this.getGroupHeadingHeight_(groupIndex) : 0) +
            rowsInCurGroup * this.getGroupItemHeight_(group.group);
        break;
      } else {
        // The index is not in the current group. Add all row heights in this
        // group to the final result.
        const groupItemCount = group.endIndex - group.startIndex + 1;
        const groupRowCount = Math.ceil(groupItemCount / this.columns);
        top += this.getGroupHeadingHeight_(groupIndex) +
            groupRowCount * this.getGroupItemHeight_(group.group);
        totalItemCount += groupItemCount;
      }
    }
    return top;
  }

  /**
   * @override
   */
  getItemRow(index) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let rows = 0;
    let totalItemCount = 0;
    for (const group of groupBySnapshot) {
      if (index <= group.endIndex) {
        // The index falls into the current group. Calculates how many rows
        // we have in the current group up until this index.
        const indexInCurGroup = index - totalItemCount;
        rows += Math.floor(indexInCurGroup / this.columns);
        break;
      } else {
        // The index is not in the current group. Add all rows in this
        // group to the final result.
        const groupItemCount = group.endIndex - group.startIndex + 1;
        rows += Math.ceil(groupItemCount / this.columns);
        totalItemCount += groupItemCount;
      }
    }
    return rows;
  }

  /**
   * Returns the column of an item which has given index.
   * @param {number} index The item index.
   */
  getItemColumn(index) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let totalItemCount = 0;
    for (const group of groupBySnapshot) {
      if (index <= group.endIndex) {
        // The index falls into the current group. Calculates the column index
        // with the remaining index in this group.
        const indexInCurGroup = index - totalItemCount;
        return indexInCurGroup % this.columns;
      }
      const groupItemCount = group.endIndex - group.startIndex + 1;
      totalItemCount += groupItemCount;
    }
    return 0;
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
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let curRow = 0;
    let index = 0;
    for (const group of groupBySnapshot) {
      const groupItemCount = group.endIndex - group.startIndex + 1;
      const groupRowCount = Math.ceil(groupItemCount / this.columns);
      if (row < curRow + groupRowCount) {
        // The row falls into the current group. Calculate the index based on
        // the column value and return.
        const isLastRowInGroup = row === curRow + groupRowCount - 1;
        const itemCountInLastRow =
            groupItemCount - (groupRowCount - 1) * this.columns;
        if (isLastRowInGroup && column >= itemCountInLastRow) {
          // column is larger than the item count in this row, return -1.
          // This happens when we try to find the index for the above/below
          // items. For example:
          // --------------------------------------
          // item 0 item 1 item 2
          // item 3 (end of group)
          // item 4 item 5 (end of group)
          // --------------------------------------
          // * To find above index for item 5, we pass (row - 1, col), col is
          //   not existed in the above row.
          // * To find the below index for item 2, we pass (row + 1, col), col
          //   is not existed in the below row.
          return -1;
        }
        return index + (row - curRow) * this.columns + column;
      }
      curRow += groupRowCount;
      index = group.endIndex + 1;
    }
    // `row` index is larger than the last row, return -1.
    return -1;
  }

  /**
   * @override
   */
  getFirstItemInRow(row) {
    if (row < 0) {
      return 0;
    }
    const index = this.getItemIndex(row, 0);
    return index === -1 ? this.dataModel.length : index;
  }

  /**
   * @override
   */
  scrollIndexIntoView(index) {
    const dataModel = this.dataModel;
    if (!dataModel || index < 0 || index >= dataModel.length) {
      return;
    }

    const itemHeight = this.getItemHeightByIndex_(index);
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
    // Render 1 more row above to make the scrolling more smooth.
    const beginRow = this.getRowForListOffset_(scrollTop) - 1;
    // Render 1 more rows below, +2 here because "endIndex" is the first item
    // of the row, in order to render the whole +1 row, we need to make sure
    // the "endIndex" is the first item of +2 row.
    const endRow = this.getRowForListOffset_(scrollTop + clientHeight - 1) + 2;
    const beginIndex = Math.max(0, this.getFirstItemInRow(beginRow));
    const endIndex =
        Math.min(this.getFirstItemInRow(endRow), this.dataModel.length);
    const result = {
      // beginIndex + 1 here because "first" will be -1 when it's being
      // consumed in redraw() method in the parent class.
      first: beginIndex + 1,
      length: endIndex - beginIndex - 1,
      last: endIndex - 1,
    };
    return result;
  }

  /**
   * @override
   */
  getAfterFillerHeight(lastIndex) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    // Excluding the current index, because [firstIndex, lastIndex) is used
    // in mergeItems().
    const index = lastIndex - 1;

    let afterFillerHeight = 0;
    let totalItemCount = 0;
    let shouldAdd = false;
    // Find the group of "index" and accumulate the height after that group.
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex];
      const groupItemCount = group.endIndex - group.startIndex + 1;
      const groupRowCount = Math.ceil(groupItemCount / this.columns);
      if (shouldAdd) {
        afterFillerHeight += this.getGroupHeadingHeight_(groupIndex) +
            groupRowCount * this.getGroupItemHeight_(group.group);
      } else if (index <= group.endIndex) {
        // index falls into the current group. Starting from this group we need
        // to add all remaining group heights into the final result.
        const indexInCurGroup = Math.max(0, index - totalItemCount);
        // For current group, we need to add the row heights starting from the
        // row which current index locates.
        afterFillerHeight +=
            (groupRowCount - Math.floor(indexInCurGroup / this.columns)) *
            this.getGroupItemHeight_(group.group);
        shouldAdd = true;
      }
      totalItemCount += groupItemCount;
    }
    return afterFillerHeight;
  }

  /**
   * Returns the height of folder items in grid view.
   * @return {number} The height of folder items.
   */
  getFolderItemHeight_() {
    // Align with CSS value for .thumbnail-item.directory: height + margin +
    // border.
    const height = util.isJellyEnabled() ? 48 : 40;
    return height + this.getItemMarginTop_() + 2;
  }

  /**
   * Returns the height of file items in grid view.
   * @return {number} The height of file items.
   */
  getFileItemHeight_() {
    // Align with CSS value for .thumbnail-item: height + margin + border.
    return 160 + this.getItemMarginTop_() + 2;
  }

  /**
   * Returns the height of group heading.
   *
   * @param {number} groupIndex
   * @return {number}
   * @private
   */
  getGroupHeadingHeight_(groupIndex) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    // For FilesRefresh, we have an additional margin for non-first group, check
    // the CSS rule ".grid-title ~ .grid-title" for more information in the CSS
    // file.
    const groupMarginTop =
        util.isJellyEnabled() && groupIndex > 0 ? GROUP_MARGIN_TOP : 0;
    switch (fileListModel.groupByField) {
      case GROUP_BY_FIELD_DIRECTORY:
        return DIRECTORY_GROUP_HEADING_HEIGHT + groupMarginTop;
      case GROUP_BY_FIELD_MODIFICATION_TIME:
        return MODIFICATION_TIME_GROUP_HEADING_HEIGHT + groupMarginTop;
      default:
        return 0;
    }
  }

  /**
   * Returns the height of the item in the group based on the group value.
   * @param {GroupValue|undefined} groupValue
   * @return {number}
   * @private
   */
  getGroupItemHeight_(groupValue) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    switch (fileListModel.groupByField) {
      case GROUP_BY_FIELD_DIRECTORY:
        return groupValue === true ? this.getFolderItemHeight_() :
                                     this.getFileItemHeight_();
      case GROUP_BY_FIELD_MODIFICATION_TIME:
        return this.getFileItemHeight_();
      default:
        return this.getFileItemHeight_();
    }
  }

  /**
   * Returns the height of the item specified by the index.
   * @param {number} index
   * @return {number}
   * @private
   */
  getItemHeightByIndex_(index) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    if (fileListModel.groupByField === GROUP_BY_FIELD_MODIFICATION_TIME) {
      return this.getFileItemHeight_();
    }

    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    for (const group of groupBySnapshot) {
      if (index <= group.endIndex) {
        // The index falls into the current group, return group item height
        // by its group value.
        return this.getGroupItemHeight_(group.group);
      }
    }
    return this.getFileItemHeight_();
  }

  /**
   * Returns the width of grid items.
   * @return {number}
   */
  getItemWidth_() {
    // Align with CSS value for .thumbnail-item: width + margin + border.
    const width = util.isJellyEnabled() ? 160 : 180;
    return width + this.getItemMarginLeft_() + 2;
  }

  /**
   * Returns the margin top of grid items.
   * @return {number};
   */
  getItemMarginTop_() {
    // Align with CSS value for .thumbnail-item: margin-top.
    return 16;
  }

  /**
   * Returns the margin left of grid items.
   * @return {number}
   */
  getItemMarginLeft_() {
    // Align with CSS value for .thumbnail-item: margin-inline-start.
    return 16;
  }

  /**
   * Returns index of a row which contains the given y-position(offset).
   * @param {number} offset The offset from the top of grid.
   * @return {number} Row index corresponding to the given offset.
   * @private
   */
  getRowForListOffset_(offset) {
    const innerOffset = Math.max(0, offset - this.paddingTop_);
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    // Loop through all the groups, calculate the accumulated height for all
    // items (item height + group heading height), until the total height
    // reaches "offset", then we know how many items can be included in this
    // offset.
    let currentHeight = 0;
    let curRow = 0;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex];
      const groupItemCount = group.endIndex - group.startIndex + 1;
      const groupRowCount = Math.ceil(groupItemCount / this.columns);
      const groupHeight = this.getGroupHeadingHeight_(groupIndex) +
          groupRowCount * this.getGroupItemHeight_(group.group);

      if (currentHeight + groupHeight > innerOffset) {
        // Current offset falls into the current group. Calculates how many
        // rows in the offset within the group.
        const offsetInCurGroup = Math.max(
            0,
            innerOffset - currentHeight -
                this.getGroupHeadingHeight_(groupIndex));
        return curRow +
            Math.floor(
                offsetInCurGroup / this.getGroupItemHeight_(group.group));
      }
      currentHeight += groupHeight;
      curRow += groupRowCount;
    }
    return this.getItemRow(fileListModel.length - 1);
  }

  /**
   * @override
   */
  createSelectionController(sm) {
    return new FileGridSelectionController(assert(sm), this);
  }

  updateGroupHeading_() {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    if (fileListModel &&
        fileListModel.groupByField === GROUP_BY_FIELD_MODIFICATION_TIME) {
      // TODO(crbug.com/1353650): find a way to update heading instead of
      // redraw.
      this.redraw();
    }
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
      let listItem = this.getListItemAncestor(box);
      const entry = listItem && this.dataModel.item(listItem.listIndex);
      if (!entry || urls.indexOf(entry.toURL()) === -1) {
        continue;
      }

      listItem = /** @type {!FileGrid.Item} */ (listItem);
      this.decorateThumbnailBox_(listItem, entry);
      this.updateSharedStatus_(listItem, entry);
      const metadata = this.metadataModel_.getCache(
                           [entry],
                           [
                             'availableOffline',
                             'pinned',
                             'canPin',
                             'syncStatus',
                             'progress',
                             'syncCompletedTime',
                           ])[0] ||
          {};
      filelist.updateInlineStatus(listItem, metadata);
      listItem.toggleAttribute(
          'disabled',
          filelist.isDlpBlocked(
              entry, assert(this.metadataModel_), assert(this.volumeManager_)));
    }
    this.updateGroupHeading_();
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
      filelist.decorateListItem(
          li, entry, assert(this.metadataModel_), assert(this.volumeManager_));
    }

    const frame = li.ownerDocument.createElement('div');
    frame.className = 'thumbnail-frame';
    li.appendChild(frame);


    const box = li.ownerDocument.createElement('div');
    box.classList.add('img-container', 'no-thumbnail');
    frame.appendChild(box);

    const bottom = li.ownerDocument.createElement('div');
    bottom.className = 'thumbnail-bottom';

    const metadata = this.metadataModel_.getCache(
                         [entry],
                         [
                           'contentMimeType',
                           'availableOffline',
                           'pinned',
                           'canPin',
                           'syncStatus',
                           'progress',
                           'syncCompletedTime',
                         ])[0] ||
        {};

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const detailIcon = filelist.renderFileTypeIcon(
        li.ownerDocument, entry, locationInfo, metadata.contentMimeType);

    // For FilesNg we add the checkmark in the same location.
    const checkmark = li.ownerDocument.createElement('div');
    checkmark.className = 'detail-checkmark';
    detailIcon.appendChild(checkmark);
    bottom.appendChild(detailIcon);
    if (util.isDriveShortcutsEnabled()) {
      bottom.appendChild(filelist.renderIconBadge(li.ownerDocument));
    }
    bottom.appendChild(
        filelist.renderFileNameLabel(li.ownerDocument, entry, locationInfo));
    frame.appendChild(bottom);
    li.setAttribute('file-name', util.getEntryLabel(locationInfo, entry));

    if (locationInfo && locationInfo.isDriveBased) {
      const inlineStatus = li.ownerDocument.createElement('xf-inline-status');
      inlineStatus.classList.add('tast-inline-status');
      frame.appendChild(inlineStatus);
    }

    if (entry) {
      this.decorateThumbnailBox_(assertInstanceof(li, HTMLLIElement), entry);
    }
    this.updateSharedStatus_(li, entry);
    filelist.updateInlineStatus(li, metadata);
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
   * @private
   */
  onSorted_() {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const hasGroupHeadingAfterSort = fileListModel.shouldShowGroupHeading();
    // Sort doesn't trigger redraw sometimes, e.g. if we sort by Name for now,
    // then we sort by time, if the list order doesn't change, no permuted event
    // is triggered, thus no redraw is triggered. In this scenario, we need to
    // manually trigger a redraw to remove/add the group heading.
    if (hasGroupHeadingAfterSort !== fileListModel.hasGroupHeadingBeforeSort) {
      this.redraw();
    }
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
        width > FileGrid.GridSize() || height > FileGrid.GridSize()) {
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
      // There is no space to show the thumbnail so don't adde one for Jelly.
      if (!util.isJellyEnabled()) {
        box.setAttribute('generic-thumbnail', 'folder');
      }
    } else if (FileType.isEncrypted(entry, opt_mimeType)) {
      box.setAttribute('generic-thumbnail', 'encrypted');
    } else {
      box.classList.toggle('no-thumbnail', true);
      const locationInfo = this.volumeManager_.getLocationInfo(entry);
      const rootType = locationInfo && locationInfo.rootType || undefined;
      const icon = FileType.getIcon(entry, opt_mimeType, rootType);
      box.setAttribute('generic-thumbnail', icon);
    }
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
   * If `isStart` is true, this returns index of the first row in which
   * bottom of grid items is greater than or equal to y. Otherwise, this returns
   * index of the last row in which top of grid items is less than or equal to
   * y.
   * @param {number} y
   * @param {boolean} isStart
   * @return {number}
   * @private
   */
  getHitRowIndex_(y, isStart) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let currentHeight = 0;
    let curRow = 0;
    const shift = isStart ? 0 : -this.getItemMarginTop_();
    const yAfterShift = y + shift;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex];
      const groupItemCount = group.endIndex - group.startIndex + 1;
      const groupRowCount = Math.ceil(groupItemCount / this.columns);
      const groupHeight = this.getGroupHeadingHeight_(groupIndex) +
          groupRowCount * this.getGroupItemHeight_(group.group);
      if (yAfterShift < currentHeight + groupHeight) {
        // The y falls into the current group.
        const yInCurGroup = yAfterShift - currentHeight -
            this.getGroupHeadingHeight_(groupIndex);
        if (yInCurGroup < 0) {
          // The remaining y in this group can't cover the current group
          // heading height.
          return isStart ? curRow : curRow - 1;
        }
        return Math.min(
            curRow + groupRowCount - 1,
            curRow +
                Math.floor(
                    yInCurGroup / this.getGroupItemHeight_(group.group)));
      }
      currentHeight += groupHeight;
      curRow += groupRowCount;
    }
    return curRow;
  }

  /**
   * Returns the index of column corresponding to the given x position.
   *
   * If `isStart` is true, this returns index of the first column in which
   * left of grid items is greater than or equal to x. Otherwise, this returns
   * index of the last column in which right of grid items is less than or equal
   * to x.
   * @param {number} x
   * @param {boolean} isStart
   * @return {number}
   * @private
   */
  getHitColumnIndex_(x, isStart) {
    const itemWidth = this.getItemWidth_();
    const shift = isStart ? 0 : -this.getItemMarginLeft_();
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

    const firstRow = this.getHitRowIndex_(top, /* isStart= */ true);
    const lastRow = this.getHitRowIndex_(bottom, /* isStart= */ false);
    const firstColumn = this.getHitColumnIndex_(startX, /* isStart= */ true);
    const lastColumn = this.getHitColumnIndex_(endX, /* isStart= */ false);

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
 * Grid size, in "px".
 * @return {number}
 */
FileGrid.GridSize = () => {
  return util.isJellyEnabled() ? 160 : 180;
};

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
      // The row (index `row + 1`) doesn't exist or doesn't have the enough
      // columns to get the column (index `col`), and `row + 1` must be the
      // last row of the group. We just need to return the last index of that
      // group.
      const fileListModel = /** @type {FileListModel} */ (grid.dataModel);
      const groupBySnapshot = fileListModel.getGroupBySnapshot();
      let curRow = 0;
      for (const group of groupBySnapshot) {
        const groupItemCount = group.endIndex - group.startIndex + 1;
        const groupRowCount = Math.ceil(groupItemCount / grid.columns);
        if (row + 1 < curRow + groupRowCount) {
          // The row falls into the current group. Return the last index in the
          // current group.
          return group.endIndex;
        }
        curRow += groupRowCount;
      }
      return grid.dataModel.length - 1;
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
    // First row, no items above, just return the first index.
    if (row - 1 < 0) {
      return 0;
    }
    const col = grid.getItemColumn(index);
    const nextIndex = grid.getItemIndex(row - 1, col);
    if (nextIndex === -1) {
      // The row (index `row - 1`) doesn't have the enough columns to get the
      // column (index `col`), we need to find the last index on "row - 1".
      return grid.getFirstItemInRow(row) - 1;
    }
    return nextIndex;
  }
}
