// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {RateLimiter} from '../../../common/js/async_util.js';
import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {maybeShowTooltip} from '../../../common/js/dom_utils.js';
import {entriesToURLs} from '../../../common/js/entry_utils.js';
import {getIcon, getType, isEncrypted} from '../../../common/js/file_type.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {getEntryLabel, str} from '../../../common/js/translations.js';
import type {FilesTooltip} from '../../elements/files_tooltip.js';
import {type FileListModel, GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME, type GroupValue} from '../file_list_model.js';
import type {ListThumbnailLoader} from '../list_thumbnail_loader.js';
import {type ThumbnailLoadedEvent} from '../list_thumbnail_loader.js';
import {type MetadataItem} from '../metadata/metadata_item.js';
import {type MetadataModel} from '../metadata/metadata_model.js';

import type {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {decorateListItem, focusParentList, handleKeyDown, handlePointerDownUp, handleTap, isDlpBlocked, renderFileNameLabel, renderFileTypeIcon, renderIconBadge, updateCacheItemInlineStatus, updateInlineStatus} from './file_table_list.js';
import {FileTapHandler} from './file_tap_handler.js';
import {Grid, GridSelectionController} from './grid.js';
import {List} from './list.js';
import {ListItem} from './list_item.js';
import type {ListSelectionModel} from './list_selection_model.js';

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
  private paddingTop_: number = 0;
  private paddingStart_: number = 0;
  private beginIndex_: number = 0;
  private endIndex_: number = 0;
  private metadataModel_: MetadataModel|null = null;
  private listThumbnailLoader_: ListThumbnailLoader|null = null;
  private volumeManager_: VolumeManager|null = null;
  private relayoutRateLimiter_: RateLimiter|null = null;
  private onThumbnailLoadedBound_: null|EventListener = null;
  a11y: A11yAnnounce|null = null;

  override get dataModel() {
    return super.dataModel as FileListModel;
  }

  override set dataModel(model: FileListModel|null) {
    // The setter for dataModel is overridden to remove/add the 'splice'
    // listener for the current data model.
    if (this.dataModel) {
      this.dataModel.removeEventListener('splice', this.onSplice_.bind(this));
      this.dataModel.removeEventListener('sorted', this.onSorted_.bind(this));
    }
    super.dataModel = model;
    if (this.dataModel) {
      this.dataModel.addEventListener('splice', this.onSplice_.bind(this));
      this.dataModel.addEventListener('sorted', this.onSorted_.bind(this));
    }
  }

  /**
   * Decorates an HTML element to be a FileGrid.
   */
  static decorate(
      element: HTMLElement, metadataModel: MetadataModel,
      volumeManager: VolumeManager, a11y: A11yAnnounce) {
    const self = element as FileGrid;
    Object.setPrototypeOf(self, FileGrid.prototype);
    self.initialize();
    self.setAttribute('aria-multiselectable', 'true');
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
    self.onThumbnailLoadedBound_ =
        self.onThumbnailLoaded_.bind(self) as EventListener;

    self.itemConstructor = function(entry: Entry) {
      const item = self.ownerDocument.createElement('li') as FileGridItem;
      self.decorateThumbnail_(item, entry);
      crInjectTypeAndInit(item, FileGridItem);
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
        (e) => updateCacheItemInlineStatus(
            e.detail, self.dataModel!, self.metadataModel_!));
  }

  private onMouseOver_(event: MouseEvent) {
    this.maybeShowToolTip(event);
  }

  maybeShowToolTip(event: Event) {
    let target = null;
    for (const element of event.composedPath()) {
      const el = element as HTMLElement;
      if (el.classList?.contains('thumbnail-item')) {
        target = el;
        break;
      }
    }
    if (!target) {
      return;
    }
    const labelElement = target.querySelector<HTMLElement>('.filename-label')!;
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, labelElement.innerText);
  }

  /**
   * @param index Index of the list item.
   */
  getItemLabel(index: number): string {
    if (index === -1) {
      return '';
    }

    const entry: Entry|null = this.dataModel?.item(index) as Entry;
    if (!entry) {
      return '';
    }

    const locationInfo = this.volumeManager_!.getLocationInfo(entry);
    return getEntryLabel(locationInfo, entry);
  }

  /**
   * Sets list thumbnail loader.
   */
  setListThumbnailLoader(listThumbnailLoader: ListThumbnailLoader|null) {
    if (this.listThumbnailLoader_) {
      this.listThumbnailLoader_.removeEventListener(
          'thumbnailLoaded', this.onThumbnailLoadedBound_);
    }

    this.listThumbnailLoader_ = listThumbnailLoader;

    if (this.listThumbnailLoader_) {
      this.listThumbnailLoader_.addEventListener(
          'thumbnailLoaded', this.onThumbnailLoadedBound_!);
      this.listThumbnailLoader_.setHighPriorityRange(
          this.beginIndex_, this.endIndex_);
    }
  }

  /**
   * Returns the element containing the thumbnail of a certain list item as
   * background image.
   * @param index The index of the item containing the desired thumbnail.
   * @return  The element containing the thumbnail, or null, if an error
   *     occurred.
   */
  getThumbnail(index: number): HTMLElement|null {
    return this.getListItemByIndex(index)
               ?.querySelector('.img-container')
               ?.querySelector('.thumbnail') ??
        null;
  }

  private onThumbnailLoaded_(event: ThumbnailLoadedEvent) {
    assert(this.dataModel);
    assert(this.metadataModel_);
    const listItem = this.getListItemByIndex(event.detail.index);
    const entry = listItem && this.dataModel.item(listItem.listIndex);
    if (!entry) {
      return;
    }
    const box = listItem.querySelector('.img-container');
    if (box) {
      const mimeType =
          this.metadataModel_.getCache(
                                 [entry],
                                 ['contentMimeType'])[0]!.contentMimeType;
      if (!event.detail.dataUrl) {
        FileGrid.clearThumbnailImage_(assertInstanceof(box, HTMLDivElement));
        this.setGenericThumbnail_(
            assertInstanceof(box, HTMLDivElement), entry, mimeType);
      } else {
        assert(event.detail.width);
        assert(event.detail.height);
        FileGrid.setThumbnailImage_(
            assertInstanceof(box, HTMLDivElement), entry, event.detail.dataUrl,
            event.detail.width, event.detail.height, mimeType);
      }
    }
    listItem.classList.toggle('thumbnail-loaded', !!event.detail.dataUrl);
  }

  override mergeItems(beginIndex: number, endIndex: number) {
    List.prototype.mergeItems.call(this, beginIndex, endIndex);
    const fileListModel = this.dataModel;
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
      const isSelected = !!this.selectionModel?.getIndexSelected(i);
      if (item.selected !== isSelected) {
        item.selected = isSelected;
      }
      // Check if index i is the start of a new group.
      if (startIndexToGroupLabel.has(i)) {
        // For first item in each group, we add a title div before the element.
        const title = document.createElement('div');
        title.setAttribute('role', 'heading');
        title.innerText = startIndexToGroupLabel.get(i)!.label;
        title.classList.add(
            'grid-title', `group-by-${fileListModel!.groupByField}`);
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

  override getItemTop(index: number) {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let top = 0;
    let totalItemCount = 0;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex]!;
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

  override getItemRow(index: number) {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
   * @param index The item index.
   */
  getItemColumn(index: number) {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
   * @param row The row index.
   * @param column The column index.
   */
  getItemIndex(row: number, column: number) {
    if (row < 0 || column < 0 || column >= this.columns) {
      return -1;
    }
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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

  override getFirstItemInRow(row: number) {
    if (row < 0) {
      return 0;
    }
    const index = this.getItemIndex(row, 0);
    return index === -1 ? this.dataModel!.length : index;
  }

  override scrollIndexIntoView(index: number) {
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

  override getItemsInViewPort(scrollTop: number, clientHeight: number) {
    // Render 1 more row above to make the scrolling more smooth.
    const beginRow = this.getRowForListOffset_(scrollTop) - 1;
    // Render 1 more rows below, +2 here because "endIndex" is the first item
    // of the row, in order to render the whole +1 row, we need to make sure
    // the "endIndex" is the first item of +2 row.
    const endRow = this.getRowForListOffset_(scrollTop + clientHeight - 1) + 2;
    const beginIndex = Math.max(0, this.getFirstItemInRow(beginRow));
    const endIndex =
        Math.min(this.getFirstItemInRow(endRow), this.dataModel!.length);
    const result = {
      // beginIndex + 1 here because "first" will be -1 when it's being
      // consumed in redraw() method in the parent class.
      first: beginIndex + 1,
      length: endIndex - beginIndex - 1,
      last: endIndex - 1,
    };
    return result;
  }

  override getAfterFillerHeight(lastIndex: number) {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
      const group = groupBySnapshot[groupIndex]!;
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
   * @return The height of folder items.
   */
  private getFolderItemHeight_(): number {
    // Align with CSS value for .thumbnail-item.directory: height + margin +
    // border.
    const height = 48;
    return height + this.getItemMarginTop_() + 2;
  }

  /**
   * Returns the height of file items in grid view.
   * @return The height of file items.
   */
  private getFileItemHeight_() {
    // Align with CSS value for .thumbnail-item: height + margin + border.
    return 160 + this.getItemMarginTop_() + 2;
  }

  /**
   * Returns the height of group heading.
   */
  private getGroupHeadingHeight_(groupIndex: number): number {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
    // We have an additional margin for non-first group, check
    // the CSS rule ".grid-title ~ .grid-title" for more information in the CSS
    // file.
    const groupMarginTop = groupIndex > 0 ? GROUP_MARGIN_TOP : 0;
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
   */
  private getGroupItemHeight_(groupValue?: GroupValue): number {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
   */
  protected override getItemHeightByIndex_(index: number): number {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
   */
  private getItemWidth_(): number {
    // Align with CSS value for .thumbnail-item: width + margin + border.
    const width = 160;
    return width + this.getItemMarginLeft_() + 2;
  }

  /**
   * Returns the margin top of grid items.
   */
  private getItemMarginTop_(): number {
    // Align with CSS value for .thumbnail-item: margin-top.
    return 16;
  }

  /**
   * Returns the margin left of grid items.
   */
  private getItemMarginLeft_(): number {
    // Align with CSS value for .thumbnail-item: margin-inline-start.
    return 16;
  }

  /**
   * Returns index of a row which contains the given y-position(offset).
   * @param offset The offset from the top of grid.
   * @return Row index corresponding to the given offset.
   */
  private getRowForListOffset_(offset: number): number {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
    const innerOffset = Math.max(0, offset - this.paddingTop_);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    // Loop through all the groups, calculate the accumulated height for all
    // items (item height + group heading height), until the total height
    // reaches "offset", then we know how many items can be included in this
    // offset.
    let currentHeight = 0;
    let curRow = 0;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex]!;
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

  override createSelectionController(sm: ListSelectionModel):
      GridSelectionController {
    assert(sm);
    return new FileGridSelectionController(sm, this);
  }

  private updateGroupHeading_() {
    const fileListModel = this.dataModel;
    if (fileListModel &&
        fileListModel.groupByField === GROUP_BY_FIELD_MODIFICATION_TIME) {
      // TODO(crbug.com/1353650): find a way to update heading instead of
      // redraw.
      this.redraw();
    }
  }

  /**
   * Updates items to reflect metadata changes.
   * @param _type Type of metadata changed.
   * @param entries Entries whose metadata changed.
   */
  updateListItemsMetadata(_type: string, entries: Array<Entry|FilesAppEntry>) {
    const urls = entriesToURLs(entries);
    const boxes =
        Array.from(this.querySelectorAll<HTMLElement>('.img-container'));
    assert(this.metadataModel_);
    assert(this.volumeManager_);
    for (const box of boxes) {
      const listItem = this.getListItemAncestor(box)!;
      const entry = listItem && this.dataModel!.item(listItem.listIndex);
      if (!entry || urls.indexOf(entry.toURL()) === -1) {
        continue;
      }

      this.decorateThumbnailBox_(listItem, entry);
      this.updateSharedStatus_(listItem, entry);
      const metadata = this.metadataModel_!.getCache(
                           [entry],
                           [
                             'availableOffline',
                             'pinned',
                             'canPin',
                             'syncStatus',
                             'progress',
                             'syncCompletedTime',
                           ])[0] ||
          {} as MetadataItem;
      updateInlineStatus(listItem, metadata);
      listItem.toggleAttribute(
          'disabled',
          isDlpBlocked(entry, this.metadataModel_, this.volumeManager_));
    }
    this.updateGroupHeading_();
  }

  /**
   * Redraws the UI. Skips multiple consecutive calls.
   */
  relayout() {
    this.relayoutRateLimiter_!.run();
  }

  /**
   * Redraws the UI immediately.
   */
  private relayoutImmediately_() {
    this.startBatchUpdates();
    this.columns = 0;
    this.redraw();
    this.endBatchUpdates();
    dispatchSimpleEvent(this, 'relayout');
  }

  /**
   * Decorates thumbnail.
   * @param  entry Entry to render a thumbnail for.
   */
  private decorateThumbnail_(li: ListItem, entry: Entry) {
    li.className = 'thumbnail-item';
    assert(this.metadataModel_);
    assert(this.volumeManager_);
    if (entry) {
      decorateListItem(li, entry, this.metadataModel_, this.volumeManager_);
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
        {} as MetadataItem;

    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const detailIcon = renderFileTypeIcon(
        li.ownerDocument, entry, locationInfo, metadata.contentMimeType);

    const checkmark = li.ownerDocument.createElement('div');
    checkmark.className = 'detail-checkmark';
    detailIcon.appendChild(checkmark);
    bottom.appendChild(detailIcon);
    bottom.appendChild(renderIconBadge(li.ownerDocument));
    bottom.appendChild(
        renderFileNameLabel(li.ownerDocument, entry, locationInfo));
    frame.appendChild(bottom);
    li.setAttribute('file-name', getEntryLabel(locationInfo, entry));

    if (locationInfo && locationInfo.isDriveBased) {
      const inlineStatus = li.ownerDocument.createElement('xf-inline-status');
      inlineStatus.classList.add('tast-inline-status');
      frame.appendChild(inlineStatus);
    }

    if (entry) {
      this.decorateThumbnailBox_(assertInstanceof(li, HTMLLIElement), entry);
    }
    this.updateSharedStatus_(li, entry);
    updateInlineStatus(li, metadata);
  }

  /**
   * Decorates the box containing a centered thumbnail image.
   *
   * @param li List item which contains the box to be decorated.
   * @param entry Entry which thumbnail is generating for.
   */
  private decorateThumbnailBox_(li: HTMLLIElement, entry: Entry|FilesAppEntry) {
    const box =
        assertInstanceof(li.querySelector('.img-container'), HTMLDivElement);

    if (entry.isDirectory) {
      this.setGenericThumbnail_(box, entry);
      return;
    }

    // Set thumbnail if it's already in cache, and the thumbnail data is not
    // empty.
    const thumbnailData =
        this.listThumbnailLoader_?.getThumbnailFromCache(entry);
    assert(this.metadataModel_);
    const mimeType =
        this.metadataModel_.getCache(
                               [entry],
                               ['contentMimeType'])[0]!.contentMimeType;
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
   * @param  li The grid item.
   * @param  entry File entry for the grid item.
   */
  private updateSharedStatus_(li: ListItem, entry: Entry|FilesAppEntry) {
    if (!entry.isDirectory) {
      return;
    }

    const shared =
        !!this.metadataModel_!.getCache([entry], ['shared'])[0]!.shared;
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
   */
  private onSplice_() {
    // When adjusting search parameters, |dataModel| is transiently empty.
    // Updating whether image-dominant is active at these times can cause
    // spurious changes. Avoid this problem by not updating whether
    // image-dominant is active when |dataModel| is empty.
    assert(this.dataModel);
    const fileListModel = this.dataModel;
    if (fileListModel.getFileCount() === 0 &&
        fileListModel.getFolderCount() === 0) {
      return;
    }
  }

  private onSorted_() {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
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
   * @param box A div element to hold thumbnails.
   * @param entry An entry of the thumbnail.
   * @param dataUrl Data url of thumbnail.
   * @param width Width of thumbnail.
   * @param height Height of thumbnail.
   * @param mimeType Optional mime type for the image.
   */
  private static setThumbnailImage_(
      box: HTMLDivElement, entry: Entry|FilesAppEntry, dataUrl: string,
      width: number, height: number, mimeType?: string) {
    const thumbnail = box.ownerDocument.createElement('div');
    thumbnail.classList.add('thumbnail');
    box.classList.toggle('no-thumbnail', false);

    // If the image is JPEG or the thumbnail is larger than the grid size,
    // resize it to cover the thumbnail box.
    const type = getType(entry, mimeType);
    if ((type.type === 'image' && type.subtype === 'JPEG') ||
        width > gridSize() || height > gridSize()) {
      thumbnail.style.backgroundSize = 'cover';
    }

    thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';

    const oldThumbnails = Array.from(box.querySelectorAll('.thumbnail'));
    for (const oldThumbnail of oldThumbnails) {
      box.removeChild(oldThumbnail);
    }

    box.appendChild(thumbnail);
  }

  /**
   * Clears thumbnail image from the box.
   * @param box A div element to hold thumbnails.
   */
  private static clearThumbnailImage_(box: HTMLDivElement) {
    const oldThumbnails = Array.from(box.querySelectorAll('.thumbnail'));
    for (const oldThumbnail of oldThumbnails) {
      box.removeChild(oldThumbnail);
    }
    box.classList.toggle('no-thumbnail', true);
  }

  /**
   * Sets a generic thumbnail on the box.
   * @param box A div element to hold thumbnails.
   * @param entry An entry of the thumbnail.
   * @param mimeType Optional mime type for the file.
   */
  private setGenericThumbnail_(
      box: HTMLDivElement, entry: Entry|FilesAppEntry, mimeType?: string) {
    if (isEncrypted(entry, mimeType)) {
      box.setAttribute('generic-thumbnail', 'encrypted');
      box.setAttribute('aria-label', str('ENCRYPTED_ICON_TOOLTIP'));
      document.querySelector<FilesTooltip>('files-tooltip')!.addTarget(box);
    } else {
      box.classList.toggle('no-thumbnail', true);
      const locationInfo = this.volumeManager_!.getLocationInfo(entry);
      const rootType = locationInfo && locationInfo.rootType || undefined;
      const icon = getIcon(entry, mimeType, rootType);
      box.setAttribute('generic-thumbnail', icon);
    }
  }

  /**
   * Returns whether the drag event is inside a entry in the list (and not the
   * background padding area).
   * @param event Drag start event.
   * @return True if the mouse is over an element in the list, False if it is in
   *     the background.
   */
  hasDragHitElement(event: MouseEvent): boolean {
    const pos = DragSelector.getScrolledPosition(this, event);
    if (!pos) {
      return false;
    }
    return this.getHitElements(pos.x, pos.y).length !== 0;
  }

  /**
   * Obtains if the drag selection should be start or not by referring the mouse
   * event.
   * @param event Drag start event.
   * @return True if the mouse is hit to the background of the list.
   */
  shouldStartDragSelection(event: MouseEvent): boolean {
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
   */
  private getHitRowIndex_(y: number, isStart: boolean): number {
    assert(this.dataModel);
    const fileListModel = this.dataModel;
    const groupBySnapshot = fileListModel.getGroupBySnapshot();

    let currentHeight = 0;
    let curRow = 0;
    const shift = isStart ? 0 : -this.getItemMarginTop_();
    const yAfterShift = y + shift;
    for (let groupIndex = 0; groupIndex < groupBySnapshot.length;
         groupIndex++) {
      const group = groupBySnapshot[groupIndex]!;
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
   */
  private getHitColumnIndex_(x: number, isStart: boolean): number {
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
   * @param x X coordinate value.
   * @param y Y coordinate value.
   * @param width Width of the coordinate.
   * @param height Height of the coordinate.
   * @return Indexes of the hit elements.
   */
  override getHitElements(
      x: number, y: number, width?: number, height?: number): number[] {
    const currentSelection = [];
    const startXWithPadding =
        isRTL() ? this.clientWidth - (x + (width ?? 0)) : x;
    const startX = Math.max(0, startXWithPadding - this.paddingStart_);
    const endX = startX + (width ? width - 1 : 0);
    const top = Math.max(0, y - this.paddingTop_);
    const bottom = top + (height ? height - 1 : 0);

    const firstRow = this.getHitRowIndex_(top, /* isStart= */ true);
    const lastRow = this.getHitRowIndex_(bottom, /* isStart= */ false);
    const firstColumn = this.getHitColumnIndex_(startX, /* isStart= */ true);
    const lastColumn = this.getHitColumnIndex_(endX, /* isStart= */ false);

    for (let row = firstRow; row <= lastRow; row++) {
      for (let col = firstColumn; col <= lastColumn; col++) {
        const index = this.getItemIndex(row, col);
        if (0 <= index && index < this.dataModel!.length) {
          currentSelection.push(index);
        }
      }
    }
    return currentSelection;
  }
}

/**
 * Grid size, in "px".
 */
function gridSize(): number {
  return 160;
}

class FileGridItem extends ListItem {
  /**
   * Label of the item.
   */
  override get label(): string {
    return this.querySelector('filename-label')?.textContent ?? '';
  }

  override set label(_newLabel: string) {
    // no-op setter. List calls this setter but Files app doesn't need it.
  }

  override initialize() {
    super.initialize();
    // Override the default role 'listitem' to 'option' to match the parent's
    // role (listbox).
    this.setAttribute('role', 'option');
    const nameId = this.id + '-entry-name';
    this.querySelector('.entry-name')!.setAttribute('id', nameId);
    this.querySelector('.img-container')!.setAttribute(
        'aria-labelledby', nameId);
    this.setAttribute('aria-labelledby', nameId);
  }
}

/**
 * Selection controller for the file grid.
 */
export class FileGridSelectionController extends GridSelectionController {
  private readonly tapHandler_ = new FileTapHandler();
  /**
   * @param selectionModel The selection model to interact with.
   * @param grid The grid to interact with.
   */
  constructor(selectionModel: ListSelectionModel, grid: FileGrid) {
    super(selectionModel, grid);
  }

  override handlePointerDownUp(e: PointerEvent, index: number) {
    handlePointerDownUp.call(this, e, index);
  }

  override handleTouchEvents(e: TouchEvent, index: number) {
    assert(e);
    if (this.tapHandler_.handleTouchEvents(e, index, handleTap.bind(this))) {
      focusParentList(e);
    }
  }

  override handleKeyDown(e: KeyboardEvent) {
    handleKeyDown.call(this, e);
  }

  get filesView(): FileGrid {
    return this.grid_ as FileGrid;
  }

  override getIndexBelow(index: number): number {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexAfter(index);
    }
    if (index === this.getLastIndex()) {
      return -1;
    }

    const grid = this.filesView;
    const row = grid.getItemRow(index);
    const col = grid.getItemColumn(index);
    const nextIndex = grid.getItemIndex(row + 1, col);
    if (nextIndex === -1) {
      // The row (index `row + 1`) doesn't exist or doesn't have the enough
      // columns to get the column (index `col`), and `row + 1` must be the
      // last row of the group. We just need to return the last index of that
      // group.
      assert(grid.dataModel);
      const groupBySnapshot = grid.dataModel.getGroupBySnapshot();
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
      return grid.dataModel!.length - 1;
    }
    return nextIndex;
  }

  override getIndexAbove(index: number) {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexBefore(index);
    }
    if (index === 0) {
      return -1;
    }

    const grid = this.filesView;
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
