// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {RateLimiter} from '../../../common/js/async_util.js';
import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';
import {maybeShowTooltip} from '../../../common/js/dom_utils.js';
import {entriesToURLs, isTeamDriveRoot} from '../../../common/js/entry_utils.js';
import {getType, isAudio, isEncrypted, isImage, isPDF, isRaw, isVideo} from '../../../common/js/file_type.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {isDlpEnabled} from '../../../common/js/flags.js';
import {getEntryLabel, str, strf} from '../../../common/js/translations.js';
import {FileListModel, GROUP_BY_FIELD_MODIFICATION_TIME} from '../file_list_model.js';
import type {ListThumbnailLoader} from '../list_thumbnail_loader.js';
import {type ThumbnailLoadedEvent} from '../list_thumbnail_loader.js';
import type {MetadataModel} from '../metadata/metadata_model.js';

import type {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {FileMetadataFormatter} from './file_metadata_formatter.js';
import {decorateListItem, FileTableList, isDlpBlocked, renderFileNameLabel, renderFileTypeIcon, renderIconBadge, updateCacheItemInlineStatus, updateListItemExternalProps} from './file_table_list.js';
import type {ListItem} from './list_item.js';
import {Table} from './table/table.js';
import {TableColumn} from './table/table_column.js';
import {TableColumnModel} from './table/table_column_model.js';


type ColumnWidthConfig = Record<string, {width: number}>;

interface ColumnHitResult {
  index: number;
  width: number;
  hitPosition: number;
}

/**
 * Custom column model for advanced auto-resizing.
 */
export class FileTableColumnModel extends TableColumnModel {
  private snapshot_: ColumnSnapshot|null = null;

  /**
   * Sets column width so that the column dividers move to the specified
   * position. This function also check the width of each column and keep the
   * width larger than MIN_WIDTH.
   *
   * @param newPos Positions of each column dividers.
   */
  private applyColumnPositions_(newPos: number[]) {
    // Check the minimum width and adjust the positions.
    for (let i = 0; i < newPos.length - 2; i++) {
      if (!this.columns_[i]!.visible) {
        newPos[i + 1] = newPos[i]!;
      } else if (newPos[i + 1]! - newPos[i]! < MIN_WIDTH) {
        newPos[i + 1] = newPos[i]! + MIN_WIDTH;
      }
    }
    for (let i = newPos.length - 1; i >= 2; i--) {
      if (!this.columns_[i - 1]!.visible) {
        newPos[i - 1] = newPos[i]!;
      } else if (newPos[i]! - newPos[i - 1]! < MIN_WIDTH) {
        newPos[i - 1] = newPos[i]! - MIN_WIDTH;
      }
    }
    // Set the new width of columns
    for (let i = 0; i < this.columns_.length; i++) {
      if (!this.columns_[i]!.visible) {
        this.columns_[i]!.width = 0;
      } else {
        // Make sure each cell has the minimum width. This is necessary when the
        // window size is too small to contain all the columns.
        this.columns_[i]!.width =
            Math.max(MIN_WIDTH, newPos[i + 1]! - newPos[i]!);
      }
    }
  }

  /**
   * Normalizes widths to make their sum 100% if possible. Uses the proportional
   * approach with some additional constraints.
   *
   * @param contentWidth Target width.
   */
  override normalizeWidths(contentWidth: number) {
    let totalWidth = 0;
    // Some columns have fixed width.
    for (let i = 0; i < this.columns_.length; i++) {
      totalWidth += this.columns_[i]!.width;
    }
    const positions = [0];
    let sum = 0;
    for (let i = 0; i < this.columns_.length; i++) {
      const column = this.columns_[i]!;
      sum += column.width;
      // Faster alternative to Math.floor for non-negative numbers.
      positions[i + 1] = ~~(contentWidth * sum / totalWidth);
    }
    this.applyColumnPositions_(positions);
  }

  /**
   * Handles to the start of column resizing by splitters.
   */
  handleSplitterDragStart() {
    this.initializeColumnPos();
  }

  /**
   * Handles to the end of column resizing by splitters.
   */
  handleSplitterDragEnd() {
    this.destroyColumnPos();
  }

  /**
   * Initialize a column snapshot which is used in setWidthAndKeepTotal().
   */
  initializeColumnPos() {
    this.snapshot_ = new ColumnSnapshot(this.columns_);
  }

  /**
   * Destroy the column snapshot which is used in setWidthAndKeepTotal().
   */
  destroyColumnPos() {
    this.snapshot_ = null;
  }

  /**
   * Sets the width of column while keeping the total width of table.
   * Before and after calling this method, you must initialize and destroy
   * columnPos with initializeColumnPos() and destroyColumnPos().
   * @param columnIndex Index of column that is resized.
   * @param columnWidth New width of the column.
   */
  setWidthAndKeepTotal(columnIndex: number, columnWidth: number) {
    columnWidth = Math.max(columnWidth, MIN_WIDTH);
    this.snapshot_!.setWidth(columnIndex, columnWidth);
    this.applyColumnPositions_(this.snapshot_!.newPos);

    // Notify about resizing
    dispatchSimpleEvent(this, 'resize');
  }

  /**
   * Obtains a column by the specified horizontal position.
   * @param x Horizontal position.
   * @return The object that contains column index, column width, and
   *     hitPosition where the horizontal position is hit in the column.
   */
  getHitColumn(x: number): null|ColumnHitResult {
    let i = 0;
    for (; i < this.columns_.length && x >= this.columns_[i]!.width; i++) {
      x -= this.columns_[i]!.width;
    }
    if (i >= this.columns_.length) {
      return null;
    }
    return {index: i, hitPosition: x, width: this.columns_[i]!.width};
  }

  override setVisible(index: number, visible: boolean) {
    if (index < 0 || index > this.columns_.length - 1) {
      return;
    }

    const column = this.columns_[index]!;
    if (column.visible === visible) {
      return;
    }

    // Re-layout the table.  This overrides the default column layout code in
    // the parent class.
    const snapshot = new ColumnSnapshot(this.columns_);

    column.visible = visible;

    // Keep the current column width, but adjust the other columns to
    // accommodate the new column.
    snapshot.setWidth(index, column.width);
    this.applyColumnPositions_(snapshot.newPos);
  }

  /**
   * Export a set of column widths for use by #restoreColumnWidths.  Use these
   * two methods instead of manually saving and setting column widths, because
   * doing the latter will not correctly save/restore column widths for hidden
   * columns.
   * see #restoreColumnWidths
   * @return config
   */
  exportColumnConfig(): ColumnWidthConfig {
    // Make a snapshot, and use that to compute a column layout where all the
    // columns are visible.
    const snapshot = new ColumnSnapshot(this.columns_);
    for (let i = 0; i < this.columns_.length; i++) {
      if (!this.columns_[i]!.visible) {
        snapshot.setWidth(i, this.columns_[i]!.absoluteWidth);
      }
    }
    // Export the column widths.
    const config: ColumnWidthConfig = {};
    for (let i = 0; i < this.columns_.length; i++) {
      config[this.columns_[i]!.id] = {
        width: snapshot.newPos[i + 1]! - snapshot.newPos[i]!,
      };
    }
    return config;
  }

  /**
   * Restores a set of column widths previously created by calling
   * #exportColumnConfig.
   * see #exportColumnConfig
   */
  restoreColumnConfig(config: ColumnWidthConfig) {
    // Columns must all be made visible before restoring their widths.  Save the
    // current visibility so it can be restored after.
    const visibility = [];
    for (let i = 0; i < this.columns_.length; i++) {
      visibility[i] = this.columns_[i]!.visible;
      this.columns_[i]!.visible = true;
    }

    // Do not use external setters (e.g. #setVisible, #setWidth) here because
    // they trigger layout thrash, and also try to dynamically resize columns,
    // which interferes with restoring the old column layout.
    for (const columnId in config) {
      const column = this.columns_[this.indexOf(columnId)];
      if (column) {
        // Set column width.  Ignore invalid widths.
        const width = ~~config[columnId]!.width;
        if (width > 0) {
          column.width = width;
        }
      }
    }

    // Restore column visibility.  Use setVisible here, to trigger table
    // relayout.
    for (let i = 0; i < this.columns_.length; i++) {
      this.setVisible(i, visibility[i]!);
    }
  }
}

/**
 * Customize the column header to decorate with a11y attributes that announces
 * the sorting used when clicked.
 *
 * @this {TableColumn} Bound by TableHeader before calling.
 * @param table Table being rendered.
 */
function renderHeader(this: TableColumn, table: FileTable): Element {
  const column = this;
  const container = table.ownerDocument.createElement('div');
  container.classList.add('table-label-container');

  const textElement = table.ownerDocument.createElement('span');
  textElement.setAttribute('id', `column-${column.id}`);
  textElement.textContent = column.name;
  const dm = table.dataModel;

  let sortOrder = column.defaultOrder;
  let isSorted = false;
  if (dm && dm.sortStatus.field === column.id) {
    isSorted = true;
    // Here we have to flip, because clicking will perform the opposite sorting.
    sortOrder = dm.sortStatus.direction === 'desc' ? 'asc' : 'desc';
  }

  textElement.setAttribute('aria-describedby', 'sort-column-' + sortOrder);
  textElement.setAttribute('role', 'button');
  container.appendChild(textElement);

  const icon = document.createElement('cr-icon-button');
  const iconName = sortOrder === 'desc' ? 'up' : 'down';
  icon.setAttribute('iron-icon', `files16:arrow_${iconName}_small`);
  icon.role = 'button';
  // If we're the sorting column make the icon a tab target.
  if (isSorted) {
    icon.id = 'sort-direction-button';
    icon.setAttribute('tabindex', '0');
    icon.setAttribute('aria-hidden', 'false');
    if (sortOrder === 'asc') {
      icon.setAttribute('aria-label', str('COLUMN_ASC_SORT_MESSAGE'));
    } else {
      icon.setAttribute('aria-label', str('COLUMN_DESC_SORT_MESSAGE'));
    }
  } else {
    icon.setAttribute('tabindex', '-1');
    icon.setAttribute('aria-hidden', 'true');
  }
  icon.classList.add('sort-icon', 'no-overlap');

  container.classList.toggle('not-sorted', !isSorted);
  container.classList.toggle('sorted', isSorted);

  container.appendChild(icon);

  return container;
}

/**
 * Minimum width of column. Note that is not marked private as it is used in the
 * unit tests.
 */
export const MIN_WIDTH = 40;

/**
 * A helper class for performing resizing of columns.
 */
class ColumnSnapshot {
  private columnPos_: number[];
  newPos: number[];

  /**
   */
  constructor(columns: TableColumn[]) {
    this.columnPos_ = [0];
    for (let i = 0; i < columns.length; i++) {
      this.columnPos_[i + 1] = columns[i]!.width + this.columnPos_[i]!;
    }

    /**
     * Starts off as a copy of the current column positions, but gets modified.
     */
    this.newPos = this.columnPos_.slice(0);
  }

  /**
   * Set the width of the given column.  The snapshot will keep the total width
   * of the table constant.
   */
  setWidth(index: number, width: number) {
    // Skip to resize 'selection' column
    if (index < 0 || index >= this.columnPos_.length - 1 || !this.columnPos_) {
      return;
    }

    // Round up if the column is shrinking, and down if the column is expanding.
    // This prevents off-by-one drift.
    const currentWidth = this.columnPos_[index + 1]! - this.columnPos_[index]!;
    const round = width < currentWidth ? Math.ceil : Math.floor;

    // Calculate new positions of column splitters.
    const newPosStart = this.columnPos_[index]! + width;
    const posEnd = this.columnPos_[this.columnPos_.length - 1]!;
    for (let i = 0; i < index + 1; i++) {
      this.newPos[i] = this.columnPos_[i]!;
    }
    for (let i = index + 1; i < this.columnPos_.length - 1; i++) {
      const posStart = this.columnPos_[index + 1]!;
      this.newPos[i] = (posEnd - newPosStart) *
              (this.columnPos_[i]! - posStart) / (posEnd - posStart) +
          newPosStart;
      this.newPos[i] = round(this.newPos[i]!);
    }
    this.newPos[index] = this.columnPos_[index]!;
    this.newPos[this.columnPos_.length - 1] = posEnd;
  }
}

/**
 * File list Table View.
 */
export class FileTable extends Table {
  private beginIndex_: number = 0;
  private endIndex_: number = 0;
  private listThumbnailLoader_: ListThumbnailLoader|null = null;
  private relayoutRateLimiter_: RateLimiter|null = null;
  private metadataModel_: MetadataModel|null = null;
  private formatter_: FileMetadataFormatter|null = null;
  private useModificationByMeTime_: boolean = false;
  private volumeManager_: VolumeManager|null = null;
  private lastSelection_: unknown[] = [];
  private onThumbnailLoadedBound_: null|EventListener = null;
  a11y: A11yAnnounce|null = null;

  /**
   * Decorates the element.
   * @param self Table to decorate.
   * @param metadataModel To retrieve metadata.
   * @param volumeManager To retrieve volume info.
   * @param a11y FileManagerUI to be able to announce a11y
   *     messages.
   * @param fullPage True if it's full page File Manager, False if a
   *    file open/save dialog.
   */
  static decorate(
      el: HTMLElement, metadataModel: MetadataModel,
      volumeManager: VolumeManager, a11y: A11yAnnounce, fullPage: boolean) {
    crInjectTypeAndInit(el, Table);
    Object.setPrototypeOf(el, FileTable.prototype);
    const self = el as FileTable;
    crInjectTypeAndInit(self.list, FileTableList);
    const list = self.list as FileTableList;
    list.setOnMergeItems(self.updateHighPriorityRange_.bind(self));
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

    self.useModificationByMeTime_ = false;

    const nameColumn =
        new TableColumn('name', str('NAME_COLUMN_LABEL'), fullPage ? 386 : 324);
    nameColumn.renderFunction = self.renderName_.bind(self);
    nameColumn.headerRenderFunction = renderHeader;

    const sizeColumn =
        new TableColumn('size', str('SIZE_COLUMN_LABEL'), 110, false);
    sizeColumn.renderFunction = self.renderSize_.bind(self);
    sizeColumn.defaultOrder = 'desc';
    sizeColumn.headerRenderFunction = renderHeader;

    const typeColumn =
        new TableColumn('type', str('TYPE_COLUMN_LABEL'), fullPage ? 110 : 110);
    typeColumn.renderFunction = self.renderType_.bind(self);
    typeColumn.headerRenderFunction = renderHeader;

    const modTimeColumn = new TableColumn(
        'modificationTime', str('DATE_COLUMN_LABEL'), fullPage ? 150 : 210);
    modTimeColumn.renderFunction = self.renderDate_.bind(self);
    modTimeColumn.defaultOrder = 'desc';
    modTimeColumn.headerRenderFunction = renderHeader;

    const columns = [nameColumn, sizeColumn, typeColumn, modTimeColumn];

    const columnModel = new FileTableColumnModel(columns);

    self.columnModel = columnModel;

    self.formatter_ = new FileMetadataFormatter();

    self.setRenderFunction(
        self.renderTableRow_.bind(self, self.getRenderFunction()));

    // Keep focus on the file list when clicking on the header.
    self.header.addEventListener('mousedown', e => {
      self.list.focus();
      e.preventDefault();
    });

    self.relayoutRateLimiter_ =
        new RateLimiter(self.relayoutImmediately_.bind(self));

    // Save the last selection. This is used by shouldStartDragSelection.
    self.list.addEventListener('mousedown', (_e: Event) => {
      self.lastSelection_ = self.selectionModel.selectedIndexes;
    }, true);
    self.list.addEventListener('touchstart', (_e: Event) => {
      self.lastSelection_ = self.selectionModel.selectedIndexes;
    }, true);
    list.shouldStartDragSelection = self.shouldStartDragSelection_.bind(self);

    list.addEventListener(
        'mouseover', self.onMouseOver_.bind(self), {passive: true});

    // Update the item's inline status when it's restored from List's cache.
    list.addEventListener(
        'cachedItemRestored',
        (e) => updateCacheItemInlineStatus(
            e.detail, self.dataModel, self.metadataModel_!));
  }

  private onMouseOver_(event: MouseEvent) {
    this.maybeShowToolTip(event);
  }

  maybeShowToolTip(event: MouseEvent) {
    const target = event.composedPath()[0] as HTMLElement;
    if (!target) {
      return;
    }
    if (!target.classList.contains('detail-name')) {
      return;
    }
    const labelElement = target.querySelector<HTMLElement>('.filename-label');
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, labelElement.innerText);
  }

  /**
   * Sort data by the given column. Overridden to add the a11y message after
   * sorting.
   * @param index The index of the column to sort by.
   */
  override sort(index: number) {
    const cm = this.columnModel;
    if (!this.dataModel) {
      return;
    }
    const fieldName = cm.getId(index);
    const sortStatus = this.dataModel.sortStatus;

    let sortDirection = cm.getDefaultOrder(index);
    if (sortStatus.field === fieldName) {
      // If it's sorting the column that's already sorted, we need to flip the
      // sorting order.
      sortDirection = sortStatus.direction === 'desc' ? 'asc' : 'desc';
    }

    const msgId =
        sortDirection === 'asc' ? 'COLUMN_SORTED_ASC' : 'COLUMN_SORTED_DESC';
    const msg = strf(msgId, fieldName);

    // Delegate to parent to sort.
    super.sort(index);
    this.a11y!.speakA11yMessage(msg);
  }

  /**
   */
  override onDataModelSorted() {
    const fileListModel = this.dataModel as FileListModel;
    const hasGroupHeadingAfterSort = fileListModel.shouldShowGroupHeading();
    // Sort doesn't trigger redraw sometimes, e.g. if we sort by Name for now,
    // then we sort by time, if the list order doesn't change, no permuted event
    // is triggered, thus no redraw is triggered. In this scenario, we need to
    // manually trigger a redraw to remove/add the group heading.
    if (hasGroupHeadingAfterSort !== fileListModel.hasGroupHeadingBeforeSort) {
      this.list.redraw();
    }
  }

  /**
   * Updates high priority range of list thumbnail loader based on current
   * viewport.
   *
   * @param beginIndex Begin index.
   * @param endIndex End index.
   */
  private updateHighPriorityRange_(beginIndex: number, endIndex: number) {
    // Keep these values to set range when a new list thumbnail loader is set.
    this.beginIndex_ = beginIndex;
    this.endIndex_ = endIndex;

    if (this.listThumbnailLoader_ !== null) {
      this.listThumbnailLoader_.setHighPriorityRange(beginIndex, endIndex);
    }
  }

  /**
   * Sets list thumbnail loader.
   * @param listThumbnailLoader A list thumbnail loader.
   */
  setListThumbnailLoader(listThumbnailLoader: ListThumbnailLoader|null) {
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
   * @param index The index of the item containing the desired
   *     thumbnail.
   * @return The element containing the thumbnail, or null, if an
   *     error occurred.
   */
  getThumbnail(index: number): null|Element {
    const listItem = this.getListItemByIndex(index);
    if (!listItem) {
      return null;
    }
    const container = listItem.querySelector('.detail-thumbnail');
    if (!container) {
      return null;
    }
    return container.querySelector('.thumbnail');
  }

  /**
   * Handles thumbnail loaded event.
   */
  private onThumbnailLoaded_(event: ThumbnailLoadedEvent) {
    const listItem = this.getListItemByIndex(event.detail.index);
    if (listItem) {
      const box = listItem.querySelector<HTMLDivElement>('.detail-thumbnail');
      if (box) {
        if (event.detail.dataUrl) {
          this.setThumbnailImage_(box, event.detail.dataUrl);
        } else {
          this.clearThumbnailImage_(box);
        }
        const icon = listItem.querySelector<HTMLElement>('.detail-icon')!;
        icon.classList.toggle('has-thumbnail', !!event.detail.dataUrl);
      }
    }
  }

  /**
   * Adjust column width to fit its content.
   * @param index Index of the column to adjust width.
   */
  override fitColumn(index: number) {
    const render = this.columnModel.getRenderFunction(index);
    const MAXIMUM_ROWS_TO_MEASURE = 1000;
    assert(this.dataModel);

    // Create a temporaty list item, put all cells into it and measure its
    // width. Then remove the item. It fits "list > *" CSS rules.
    const container = this.ownerDocument.createElement('li');
    container.style.display = 'inline-block';
    container.style.textAlign = 'start';
    // The container will have width of the longest cell.
    container.style.webkitBoxOrient = 'vertical';

    // Select at most MAXIMUM_ROWS_TO_MEASURE items around visible area.
    const items = this.list.getItemsInViewPort(
        this.list.scrollTop, this.list.clientHeight);
    const firstIndex = Math.floor(
        Math.max(0, (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
    const lastIndex =
        Math.min(this.dataModel.length, firstIndex + MAXIMUM_ROWS_TO_MEASURE);
    for (let i = firstIndex; i < lastIndex; i++) {
      const item = this.dataModel.item(i);
      const div = this.ownerDocument.createElement('div');
      div.className = 'table-row-cell';
      div.appendChild(render(item, this.columnModel.getId(index)!, this));
      container.appendChild(div);
    }
    this.list.appendChild(container);
    const width = parseFloat(window.getComputedStyle(container).width);
    this.list.removeChild(container);

    const cm = this.columnModel as FileTableColumnModel;
    cm.initializeColumnPos();
    cm.setWidthAndKeepTotal(index, Math.ceil(width));
    cm.destroyColumnPos();
  }

  /**
   * Sets date and time format.
   * @param use12hourClock True if 12 hours clock, False if 24 hours.
   */
  setDateTimeFormat(use12hourClock: boolean) {
    this.formatter_!.setDateTimeFormat(use12hourClock);
  }

  /**
   * Sets whether to use modificationByMeTime as "Last Modified" time.
   */
  setUseModificationByMeTime(useModificationByMeTime: boolean) {
    this.useModificationByMeTime_ = useModificationByMeTime;
  }

  /**
   * Obtains if the drag selection should be start or not by referring the mouse
   * event.
   * @param event Drag start event.
   * @return True if the mouse is hit to the background of the list,
   * or certain areas of the inside of the list that would start a drag
   * selection.
   */
  private shouldStartDragSelection_(event: MouseEvent): boolean {
    // If the shift key is pressed, it should starts drag selection.
    if (event.shiftKey) {
      return true;
    }

    const list = this.list as FileTableList;
    // If we're outside of the element list, start the drag selection.
    if (!list.hasDragHitElement(event)) {
      return true;
    }

    // If the position values are negative, it points the out of list.
    const pos = DragSelector.getScrolledPosition(this.list, event);
    if (!pos) {
      return false;
    }
    if (pos.x < 0 || pos.y < 0) {
      return true;
    }

    // If the item index is out of range, it should start the drag selection.
    const itemHeight = this.list.measureItem().height;
    // Faster alternative to Math.floor for non-negative numbers.
    const itemIndex = ~~(pos.y / itemHeight);
    const length = this.dataModel?.length ?? 0;
    if (itemIndex >= length) {
      return true;
    }

    // If the pointed item is already selected, it should not start the drag
    // selection.
    if (this.lastSelection_ && this.lastSelection_.indexOf(itemIndex) !== -1) {
      return false;
    }

    const cm = this.columnModel as FileTableColumnModel;
    // If the horizontal value is not hit to column, it should start the drag
    // selection.
    const hitColumn = cm.getHitColumn(pos.x);
    if (!hitColumn) {
      return true;
    }

    // Check if the point is on the column contents or not.
    switch (cm.getId(hitColumn.index)) {
      case 'name':
        const item = this.list.getListItemByIndex(itemIndex);
        if (!item) {
          return false;
        }

        const spanElement = item.querySelector('.filename-label span')!;
        const spanRect = spanElement && spanElement.getBoundingClientRect();
        // The `list.cachedBounds` is set by DragSelector.getScrolledPosition.
        if (!this.list.cachedBounds) {
          return true;
        }
        const textRight =
            spanRect.left - this.list.cachedBounds.left + spanRect.width;
        return textRight <= hitColumn.hitPosition;
      default:
        return true;
    }
  }

  /**
   * Render the Name column of the detail table.
   *
   * Invoked by Table when a file needs to be rendered.
   *
   * @param entry The Entry object to render.
   * @param _columnId The id of the column to be rendered.
   * @param _table The table doing the rendering.
   * @return Created element.
   */
  private renderName_(entry: Entry, _columnId: string, _table: Table):
      HTMLDivElement {
    const label = this.ownerDocument.createElement('div');

    const metadata = this.metadataModel_!.getCache(
        [entry], ['contentMimeType', 'isDlpRestricted'])[0]!;
    const mimeType = metadata.contentMimeType;
    const locationInfo = this.volumeManager_!.getLocationInfo(entry);
    const icon =
        renderFileTypeIcon(this.ownerDocument, entry, locationInfo, mimeType);
    if (isImage(entry, mimeType) || isPDF(entry, mimeType) ||
        isVideo(entry, mimeType) || isAudio(entry, mimeType) ||
        isRaw(entry, mimeType)) {
      icon.appendChild(this.renderThumbnail_(entry, icon));
    }
    icon.appendChild(this.renderCheckmark_());
    label.appendChild(icon);
    label.appendChild(renderIconBadge(this.ownerDocument));
    (label as HTMLDivElement & {entry: Entry}).entry = entry;
    label.className = 'detail-name';
    label.appendChild(
        renderFileNameLabel(this.ownerDocument, entry, locationInfo));
    if (locationInfo && locationInfo.isDriveBased) {
      const inlineStatus = this.ownerDocument.createElement('xf-inline-status');
      inlineStatus.classList.add('tast-inline-status');
      label.appendChild(inlineStatus);
    }
    return label;
  }

  /**
   * @param index Index of the list item.
   */
  getItemLabel(index: number): string {
    if (index === -1) {
      return '';
    }

    const entry = this.dataModel?.item(index) as Entry | FilesAppEntry;
    if (!entry) {
      return '';
    }

    const locationInfo = this.volumeManager_?.getLocationInfo(entry) || null;
    return getEntryLabel(locationInfo, entry);
  }

  /**
   * Render the Size column of the detail table.
   *
   * @param entry The Entry object to render.
   * @param _columnId The id of the column to be rendered.
   * @param _table The table doing the rendering.
   * @return Created element.
   */
  private renderSize_(entry: Entry, _columnId: string, _table: Table):
      HTMLDivElement {
    const div = this.ownerDocument.createElement('div');
    div.className = 'size';
    this.updateSize_(div, entry);

    return div;
  }

  /**
   * Sets up or updates the size cell.
   *
   * @param div The table cell.
   * @param entry The corresponding entry.
   */
  private updateSize_(div: HTMLElement, entry: Entry|FilesAppEntry) {
    const metadata = this.metadataModel_!.getCache(
        [entry], ['size', 'hosted', 'contentMimeType'])[0]!;
    const size = metadata.size;
    const special =
        metadata.hosted || isEncrypted(entry, metadata.contentMimeType);
    div.textContent = this.formatter_!.formatSize(size, special);
  }

  /**
   * Render the Type column of the detail table.
   *
   * @param entry The Entry object to render.
   * @param _columnId The id of the column to be rendered.
   * @param _table The table doing the rendering.
   * @return Created element.
   */
  private renderType_(entry: Entry, _columnId: string, _table: Table):
      HTMLDivElement {
    const div = this.ownerDocument.createElement('div');
    div.className = 'type';

    const mimeType =
        this.metadataModel_!.getCache(
                                [entry],
                                ['contentMimeType'])[0]!.contentMimeType;
    div.textContent = FileListModel.getFileTypeString(getType(entry, mimeType));
    return div;
  }

  /**
   * Render the Date column of the detail table.
   *
   * @param entry The Entry object to render.
   * @param _columnId The id of the column to be rendered.
   * @param _table The table doing the rendering.
   * @return Created element.
   */
  private renderDate_(entry: Entry, _columnId: string, _table: Table):
      HTMLDivElement {
    const div = this.ownerDocument.createElement('div');

    div.className = 'dateholder';
    const label = this.ownerDocument.createElement('div');
    div.appendChild(label);
    label.className = 'date';
    this.updateDate_(label, entry);
    const metadata = this.metadataModel_!.getCache(
        [entry], ['contentMimeType', 'isDlpRestricted'])[0]!;
    const encrypted = isEncrypted(entry, metadata.contentMimeType);
    if (encrypted) {
      div.appendChild(this.renderEncryptedIcon_());
    }
    if (isDlpEnabled()) {
      div.appendChild(this.renderDlpManagedIcon_(!!metadata.isDlpRestricted));
    }
    return div;
  }

  /**
   * Sets up or updates the date cell.
   *
   * @param div The table cell.
   * @param entry Entry of file to update.
   */
  private updateDate_(div: HTMLElement, entry: Entry|FilesAppEntry) {
    const item = this.metadataModel_!.getCache(
        [entry], ['modificationTime', 'modificationByMeTime'])[0]!;
    const modTime = this.useModificationByMeTime_ ?
        item.modificationByMeTime || item.modificationTime :
        item.modificationTime;

    div.textContent = this.formatter_!.formatModDate(modTime);
  }

  private updateGroupHeading_() {
    const fileListModel = this.dataModel as FileListModel;
    if (fileListModel &&
        fileListModel.groupByField === GROUP_BY_FIELD_MODIFICATION_TIME) {
      // TODO(crbug.com/1353650): find a way to update heading instead of redraw
      this.redraw();
    }
  }

  /**
   * Updates the file metadata in the table item.
   *
   * @param item Table item.
   * @param entry File entry.
   */
  updateFileMetadata(item: HTMLElement, entry: Entry|FilesAppEntry) {
    this.updateDate_(item.querySelector<HTMLElement>('.date')!, entry);
    this.updateSize_(item.querySelector<HTMLElement>('.size')!, entry);
  }

  /**
   * Updates list items 'in place' on metadata change.
   * @param type Type of metadata change.
   * @param entries Entries to update.
   */
  updateListItemsMetadata(type: string, entries: Array<Entry|FilesAppEntry>) {
    const urls = entriesToURLs(entries);
    assert(this.dataModel);
    const dataModel = this.dataModel;
    const forEachCell =
        (selector: string,
         callback: (
             cell: HTMLElement, entry: Entry|FilesAppEntry,
             item: ListItem|null) => void) => {
          const cells = this.querySelectorAll<HTMLElement>(selector);
          for (let i = 0; i < cells.length; i++) {
            const cell = cells[i]!;
            const listItem = this.list.getListItemAncestor(cell);
            const index = listItem?.listIndex ?? 0;
            const entry = dataModel.item(index);
            if (entry && urls.indexOf(entry.toURL()) !== -1) {
              callback.call(this, cell, entry, listItem);
            }
          }
        };
    if (type === 'filesystem') {
      forEachCell('.table-row-cell .date', (item, entry) => {
        this.updateDate_(item, entry);
      });
      forEachCell('.table-row-cell > .size', (item, entry) => {
        this.updateSize_(item, entry);
      });
      this.updateGroupHeading_();
    } else if (type === 'external') {
      // The cell name does not matter as the entire list item is needed.
      forEachCell(
          '.table-row-cell .date',
          (_item: HTMLElement, entry: Entry|FilesAppEntry,
           listItem: ListItem|null) => {
            updateListItemExternalProps(
                listItem!, entry,
                this.metadataModel_!.getCache(
                    [entry],
                    [
                      'availableOffline',
                      'customIconUrl',
                      'shared',
                      'isMachineRoot',
                      'isExternalMedia',
                      'hosted',
                      'pinned',
                      'syncStatus',
                      'progress',
                      'syncCompletedTime',
                      'shortcut',
                      'canPin',
                      'isDlpRestricted',
                    ])[0]!,
                isTeamDriveRoot(entry));
            listItem!.toggleAttribute(
                'disabled',
                isDlpBlocked(
                    entry, this.metadataModel_!, this.volumeManager_!));
          });
    }
  }

  /**
   * Renders table row.
   * @param baseRenderFunction Base renderer.
   * @param entry Corresponding entry.
   * @return Created element.
   */
  private renderTableRow_(
      baseRenderFunction: (entry: Entry, table: Table) => ListItem,
      entry: Entry): ListItem {
    const item = baseRenderFunction(entry, this);
    const nameId = item.id + '-entry-name';
    const sizeId = item.id + '-size';
    const typeId = item.id + '-type';
    const dateId = item.id + '-date';
    const dlpId = item.id + '-dlp-managed-icon';
    const encryptedId = item.id + '-encrypted-icon';
    decorateListItem(item, entry, this.metadataModel_!, this.volumeManager_!);
    item.setAttribute('file-name', entry.name);
    item.querySelector('.detail-name')!.setAttribute('id', nameId);
    item.querySelector('.size')!.setAttribute('id', sizeId);
    item.querySelector('.type')!.setAttribute('id', typeId);
    item.querySelector('.date')!.setAttribute('id', dateId);
    const dlpManagedIcon = item.querySelector('.dlp-managed-icon');
    if (dlpManagedIcon) {
      dlpManagedIcon.setAttribute('id', dlpId);
      this.ownerDocument.querySelector('files-tooltip')!.addTargets(
          item.querySelectorAll('.dlp-managed-icon'));
    }
    const encryptedIcon = item.querySelector('.encrypted-icon');
    if (encryptedIcon) {
      encryptedIcon.setAttribute('id', encryptedId);
    }

    item.setAttribute(
        'aria-labelledby',
        `${nameId} column-size ${sizeId} column-type ${
            typeId} column-modificationTime ${dateId} ${encryptedId}`);
    return item;
  }

  /**
   * Renders the file thumbnail in the detail table.
   * @param entry The Entry object to render.
   * @param parent The parent DOM element.
   * @return Created element.
   */
  private renderThumbnail_(entry: Entry, parent: HTMLDivElement):
      HTMLDivElement {
    const box = this.ownerDocument.createElement('div');
    box.className = 'detail-thumbnail';

    // Set thumbnail if it's already in cache.
    const thumbnailData = this.listThumbnailLoader_ ?
        this.listThumbnailLoader_.getThumbnailFromCache(entry) :
        null;
    if (thumbnailData && thumbnailData.dataUrl) {
      this.setThumbnailImage_(box, thumbnailData.dataUrl);
      parent.classList.add('has-thumbnail');
    }

    return box;
  }

  /**
   * Sets thumbnail image to the box.
   * @param box Detail thumbnail div element.
   * @param dataUrl Data url of thumbnail.
   */
  private setThumbnailImage_(box: HTMLDivElement, dataUrl: string) {
    const thumbnail = box.ownerDocument.createElement('div');
    thumbnail.classList.add('thumbnail');
    thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';
    const oldThumbnails = box.querySelectorAll('.thumbnail');

    for (let i = 0; i < oldThumbnails.length; i++) {
      box.removeChild(oldThumbnails[i]!);
    }

    box.appendChild(thumbnail);
  }

  /**
   * Clears thumbnail image from the box.
   * @param box Detail thumbnail div element.
   */
  private clearThumbnailImage_(box: HTMLDivElement) {
    const oldThumbnails = box.querySelectorAll('.thumbnail');

    for (let i = 0; i < oldThumbnails.length; i++) {
      box.removeChild(oldThumbnails[i]!);
    }
  }

  /**
   * Renders the selection checkmark in the detail table.
   * @return Created element.
   */
  private renderCheckmark_(): HTMLDivElement {
    const checkmark = this.ownerDocument.createElement('div');
    checkmark.className = 'detail-checkmark';
    return checkmark;
  }

  /**
   * Renders the DLP managed icon in the detail table.
   * @param isDlpRestricted Whether the icon should be shown.
   * @return Created element.
   */
  private renderDlpManagedIcon_(isDlpRestricted: boolean): HTMLDivElement {
    const icon = this.ownerDocument.createElement('div');
    icon.className = 'dlp-managed-icon';
    icon.toggleAttribute('has-tooltip');
    icon.dataset['tooltipLinkHref'] = str('DLP_HELP_URL');
    icon.dataset['tooltipLinkAriaLabel'] = str('DLP_MANAGED_ICON_TOOLTIP_DESC');
    icon.dataset['tooltipLinkText'] = str('DLP_MANAGED_ICON_TOOLTIP_LINK');
    icon.role = 'link';
    icon.setAttribute('aria-label', str('DLP_MANAGED_ICON_TOOLTIP'));
    icon.toggleAttribute('show-card-tooltip');
    icon.classList.toggle('is-dlp-restricted', isDlpRestricted);
    icon.toggleAttribute('aria-hidden', isDlpRestricted);
    return icon;
  }

  /**
   * Renders the encrypted icon in the detail table, used to mark Google Drive
   * CSE files.
   * @return Created element.
   */
  private renderEncryptedIcon_(): HTMLDivElement {
    const icon = this.ownerDocument.createElement('div');
    icon.className = 'encrypted-icon';
    icon.role = 'image';
    icon.setAttribute('aria-label', str('ENCRYPTED_ICON_TOOLTIP'));
    document.querySelector('files-tooltip')?.addTarget(icon);
    return icon;
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
    if (this.clientWidth > 0) {
      this.normalizeColumns();
    }
    this.redraw();
    dispatchSimpleEvent(this.list, 'relayout');
  }
}
