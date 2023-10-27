// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';

import {RateLimiter} from '../../../common/js/async_util.js';
import {maybeShowTooltip} from '../../../common/js/dom_utils.js';
import {entriesToURLs, isTeamDriveRoot} from '../../../common/js/entry_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {isDlpEnabled, isDriveShortcutsEnabled, isInlineSyncStatusEnabled, isJellyEnabled} from '../../../common/js/flags.js';
import {getEntryLabel, str, strf} from '../../../common/js/translations.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {FileListModel, GROUP_BY_FIELD_MODIFICATION_TIME} from '../file_list_model.js';
import {ListThumbnailLoader} from '../list_thumbnail_loader.js';
import {MetadataModel} from '../metadata/metadata_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {FileMetadataFormatter} from './file_metadata_formatter.js';
import {filelist, FileTableList} from './file_table_list.js';
import {Table} from './table/table.js';
import {TableColumn} from './table/table_column.js';
import {TableColumnModel} from './table/table_column_model.js';

/**
 * Custom column model for advanced auto-resizing.
 */
export class FileTableColumnModel extends TableColumnModel {
  /**
   * @param {!Array<TableColumn>} tableColumns Table columns.
   */
  constructor(tableColumns) {
    super(tableColumns);

    /** @private @type {?FileTableColumnModel.ColumnSnapshot} */
    this.snapshot_ = null;
  }

  /**
   * Sets column width so that the column dividers move to the specified
   * position. This function also check the width of each column and keep the
   * width larger than MIN_WIDTH_.
   *
   * @private
   * @param {Array<number>} newPos Positions of each column dividers.
   */
  applyColumnPositions_(newPos) {
    // Check the minimum width and adjust the positions.
    for (let i = 0; i < newPos.length - 2; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (!this.columns_[i].visible) {
        // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable
        // to type 'number'.
        newPos[i + 1] = newPos[i];
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      } else if (newPos[i + 1] - newPos[i] < FileTableColumnModel.MIN_WIDTH_) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        newPos[i + 1] = newPos[i] + FileTableColumnModel.MIN_WIDTH_;
      }
    }
    for (let i = newPos.length - 1; i >= 2; i--) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (!this.columns_[i - 1].visible) {
        // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable
        // to type 'number'.
        newPos[i - 1] = newPos[i];
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      } else if (newPos[i] - newPos[i - 1] < FileTableColumnModel.MIN_WIDTH_) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        newPos[i - 1] = newPos[i] - FileTableColumnModel.MIN_WIDTH_;
      }
    }
    // Set the new width of columns
    for (let i = 0; i < this.columns_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (!this.columns_[i].visible) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        this.columns_[i].width = 0;
      } else {
        // Make sure each cell has the minimum width. This is necessary when the
        // window size is too small to contain all the columns.
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        this.columns_[i].width = Math.max(
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            FileTableColumnModel.MIN_WIDTH_, newPos[i + 1] - newPos[i]);
      }
    }
  }

  /**
   * Normalizes widths to make their sum 100% if possible. Uses the proportional
   * approach with some additional constraints.
   *
   * @param {number} contentWidth Target width.
   * @override
   */
  normalizeWidths(contentWidth) {
    let totalWidth = 0;
    // Some columns have fixed width.
    for (let i = 0; i < this.columns_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      totalWidth += this.columns_[i].width;
    }
    const positions = [0];
    let sum = 0;
    for (let i = 0; i < this.columns_.length; i++) {
      const column = this.columns_[i];
      // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
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
    this.snapshot_ = new FileTableColumnModel.ColumnSnapshot(this.columns_);
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
   * @param {number} columnIndex Index of column that is resized.
   * @param {number} columnWidth New width of the column.
   */
  setWidthAndKeepTotal(columnIndex, columnWidth) {
    columnWidth = Math.max(columnWidth, FileTableColumnModel.MIN_WIDTH_);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.snapshot_.setWidth(columnIndex, columnWidth);
    // @ts-ignore: error TS2341: Property 'newPos' is private and only
    // accessible within class 'ColumnSnapshot'.
    this.applyColumnPositions_(this.snapshot_.newPos);

    // Notify about resizing
    dispatchSimpleEvent(this, 'resize');
  }

  /**
   * Obtains a column by the specified horizontal position.
   * @param {number} x Horizontal position.
   * @return {Object} The object that contains column index, column width, and
   *     hitPosition where the horizontal position is hit in the column.
   */
  getHitColumn(x) {
    let i = 0;
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    for (; i < this.columns_.length && x >= this.columns_[i].width; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      x -= this.columns_[i].width;
    }
    if (i >= this.columns_.length) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'Object'.
      return null;
    }
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return {index: i, hitPosition: x, width: this.columns_[i].width};
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'visible' implicitly has an 'any' type.
  setVisible(index, visible) {
    if (index < 0 || index > this.columns_.length - 1) {
      return;
    }

    const column = this.columns_[index];
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    if (column.visible === visible) {
      return;
    }

    // Re-layout the table.  This overrides the default column layout code in
    // the parent class.
    const snapshot = new FileTableColumnModel.ColumnSnapshot(this.columns_);

    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    column.visible = visible;

    // Keep the current column width, but adjust the other columns to
    // accommodate the new column.
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    snapshot.setWidth(index, column.width);
    // @ts-ignore: error TS2341: Property 'newPos' is private and only
    // accessible within class 'ColumnSnapshot'.
    this.applyColumnPositions_(snapshot.newPos);
  }

  /**
   * Export a set of column widths for use by #restoreColumnWidths.  Use these
   * two methods instead of manually saving and setting column widths, because
   * doing the latter will not correctly save/restore column widths for hidden
   * columns.
   * see #restoreColumnWidths
   * @return {!Object} config
   */
  exportColumnConfig() {
    // Make a snapshot, and use that to compute a column layout where all the
    // columns are visible.
    const snapshot = new FileTableColumnModel.ColumnSnapshot(this.columns_);
    for (let i = 0; i < this.columns_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (!this.columns_[i].visible) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        snapshot.setWidth(i, this.columns_[i].absoluteWidth);
      }
    }
    // Export the column widths.
    const config = {};
    for (let i = 0; i < this.columns_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      config[this.columns_[i].id] = {
        // @ts-ignore: error TS2341: Property 'newPos' is private and only
        // accessible within class 'ColumnSnapshot'.
        width: snapshot.newPos[i + 1] - snapshot.newPos[i],
      };
    }
    return config;
  }

  /**
   * Restores a set of column widths previously created by calling
   * #exportColumnConfig.
   * see #exportColumnConfig
   * @param {!Object} config
   */
  restoreColumnConfig(config) {
    // Convert old-style raw column widths into new-style config objects.
    if (Array.isArray(config)) {
      const tmpConfig = {};
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      tmpConfig[this.columns_[0].id] = config[0];
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      tmpConfig[this.columns_[1].id] = config[1];
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      tmpConfig[this.columns_[3].id] = config[2];
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      tmpConfig[this.columns_[4].id] = config[3];
      config = tmpConfig;
    }

    // Columns must all be made visible before restoring their widths.  Save the
    // current visibility so it can be restored after.
    const visibility = [];
    for (let i = 0; i < this.columns_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      visibility[i] = this.columns_[i].visible;
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.columns_[i].visible = true;
    }

    // Do not use external setters (e.g. #setVisible, #setWidth) here because
    // they trigger layout thrash, and also try to dynamically resize columns,
    // which interferes with restoring the old column layout.
    for (const columnId in config) {
      const column = this.columns_[this.indexOf(columnId)];
      if (column) {
        // Set column width.  Ignore invalid widths.
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'Object'.
        const width = ~~config[columnId].width;
        if (width > 0) {
          column.width = width;
        }
      }
    }

    // Restore column visibility.  Use setVisible here, to trigger table
    // relayout.
    for (let i = 0; i < this.columns_.length; i++) {
      this.setVisible(i, visibility[i]);
    }
  }
}

/**
 * Customize the column header to decorate with a11y attributes that announces
 * the sorting used when clicked.
 *
 * @this {TableColumn} Bound by TableHeader before
 * calling.
 * @param {Element} table Table being rendered.
 * @return {Element}
 */
export function renderHeader_(table) {
  const column = /** @type {TableColumn} */ (this);
  const container = table.ownerDocument.createElement('div');
  container.classList.add('table-label-container');

  const textElement = table.ownerDocument.createElement('span');
  textElement.setAttribute('id', `column-${column.id}`);
  textElement.textContent = column.name;
  // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
  // 'Element'.
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
 * @const @type {number}
 */
FileTableColumnModel.MIN_WIDTH_ = 40;

/**
 * A helper class for performing resizing of columns.
 */
FileTableColumnModel.ColumnSnapshot = class {
  /**
   * @param {!Array<!TableColumn>} columns
   */
  constructor(columns) {
    /** @private @type {!Array<number>} */
    this.columnPos_ = [0];
    for (let i = 0; i < columns.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.columnPos_[i + 1] = columns[i].width + this.columnPos_[i];
    }

    /**
     * Starts off as a copy of the current column positions, but gets modified.
     * @private @type {!Array<number>}
     */
    this.newPos = this.columnPos_.slice(0);
  }

  /**
   * Set the width of the given column.  The snapshot will keep the total width
   * of the table constant.
   * @param {number} index
   * @param {number} width
   */
  setWidth(index, width) {
    // Skip to resize 'selection' column
    if (index < 0 || index >= this.columnPos_.length - 1 || !this.columnPos_) {
      return;
    }

    // Round up if the column is shrinking, and down if the column is expanding.
    // This prevents off-by-one drift.
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    const currentWidth = this.columnPos_[index + 1] - this.columnPos_[index];
    const round = width < currentWidth ? Math.ceil : Math.floor;

    // Calculate new positions of column splitters.
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    const newPosStart = this.columnPos_[index] + width;
    const posEnd = this.columnPos_[this.columnPos_.length - 1];
    for (let i = 0; i < index + 1; i++) {
      // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable
      // to type 'number'.
      this.newPos[i] = this.columnPos_[i];
    }
    for (let i = index + 1; i < this.columnPos_.length - 1; i++) {
      const posStart = this.columnPos_[index + 1];
      // @ts-ignore: error TS18048: 'posEnd' is possibly 'undefined'.
      this.newPos[i] = (posEnd - newPosStart) *
              // @ts-ignore: error TS18048: 'posStart' is possibly 'undefined'.
              (this.columnPos_[i] - posStart) / (posEnd - posStart) +
          newPosStart;
      // @ts-ignore: error TS2345: Argument of type 'number | undefined' is not
      // assignable to parameter of type 'number'.
      this.newPos[i] = round(this.newPos[i]);
    }
    // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable to
    // type 'number'.
    this.newPos[index] = this.columnPos_[index];
    // @ts-ignore: error TS2322: Type 'number | undefined' is not assignable to
    // type 'number'.
    this.newPos[this.columnPos_.length - 1] = posEnd;
  }
};

/**
 * File list Table View.
 */
// @ts-ignore: error TS2415: Class 'FileTable' incorrectly extends base class
// 'Table'.
export class FileTable extends Table {
  constructor() {
    super();

    /** @private @type {number} */
    this.beginIndex_ = 0;

    /** @private @type {number} */
    this.endIndex_ = 0;

    /** @private @type {?ListThumbnailLoader} */
    this.listThumbnailLoader_ = null;

    /** @private @type {?RateLimiter} */
    this.relayoutRateLimiter_ = null;

    /** @private @type {?MetadataModel} */
    this.metadataModel_ = null;

    /** @private @type {?FileMetadataFormatter} */
    this.formatter_ = null;

    /** @private @type {boolean} */
    this.useModificationByMeTime_ = false;

    /** @private @type {?VolumeManager} */
    this.volumeManager_ = null;

    // @ts-ignore: error TS2314: Generic type 'Array<T>' requires 1 type
    // argument(s).
    /** @private @type {!Array} */
    this.lastSelection_ = [];

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {?function(!Event)} */
    this.onThumbnailLoadedBound_ = null;

    /** @public @type {?A11yAnnounce} */
    this.a11y = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * Decorates the element.
   * @param {!Element} self Table to decorate.
   * @param {!MetadataModel} metadataModel To retrieve metadata.
   * @param {!VolumeManager} volumeManager To retrieve volume info.
   * @param {!A11yAnnounce} a11y FileManagerUI to be able to announce a11y
   *     messages.
   * @param {boolean} fullPage True if it's full page File Manager, False if a
   *    file open/save dialog.
   * @suppress {checkPrototypalTypes} Closure was failing because the signature
   * of this decorate() doesn't match the base class.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class 'Table'.
  static decorate(self, metadataModel, volumeManager, a11y, fullPage) {
    Table.decorate(self);
    // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
    // 'Element'.
    self.__proto__ = FileTable.prototype;
    // @ts-ignore: error TS2339: Property 'list' does not exist on type
    // 'Element'.
    FileTableList.decorate(self.list);
    // @ts-ignore: error TS2339: Property 'updateHighPriorityRange_' does not
    // exist on type 'Element'.
    self.list.setOnMergeItems(self.updateHighPriorityRange_.bind(self));
    // @ts-ignore: error TS2339: Property 'metadataModel_' does not exist on
    // type 'Element'.
    self.metadataModel_ = metadataModel;
    // @ts-ignore: error TS2339: Property 'volumeManager_' does not exist on
    // type 'Element'.
    self.volumeManager_ = volumeManager;
    // @ts-ignore: error TS2339: Property 'a11y' does not exist on type
    // 'Element'.
    self.a11y = a11y;

    // Force the list's ending spacer to be tall enough to allow overscroll.
    const endSpacer = self.querySelector('.spacer:last-child');
    if (endSpacer) {
      endSpacer.classList.add('signals-overscroll');
    }

    /** @private @type {ListThumbnailLoader} */
    // @ts-ignore: error TS2339: Property 'listThumbnailLoader_' does not exist
    // on type 'Element'.
    self.listThumbnailLoader_ = null;

    /** @private @type {number} */
    // @ts-ignore: error TS2339: Property 'beginIndex_' does not exist on type
    // 'Element'.
    self.beginIndex_ = 0;

    /** @private @type {number} */
    // @ts-ignore: error TS2339: Property 'endIndex_' does not exist on type
    // 'Element'.
    self.endIndex_ = 0;

    // @ts-ignore: error TS7014: Function type, which lacks return-type
    // annotation, implicitly has an 'any' return type.
    /** @private @type {function(!Event)} */
    // @ts-ignore: error TS2339: Property 'onThumbnailLoaded_' does not exist on
    // type 'Element'.
    self.onThumbnailLoadedBound_ = self.onThumbnailLoaded_.bind(self);

    /** @private @type {boolean} */
    // @ts-ignore: error TS2339: Property 'useModificationByMeTime_' does not
    // exist on type 'Element'.
    self.useModificationByMeTime_ = false;

    const nameColumn =
        new TableColumn('name', str('NAME_COLUMN_LABEL'), fullPage ? 386 : 324);
    // @ts-ignore: error TS2339: Property 'renderName_' does not exist on type
    // 'Element'.
    nameColumn.renderFunction = self.renderName_.bind(self);
    nameColumn.headerRenderFunction = renderHeader_;

    const sizeColumn = new TableColumn(
        'size', str('SIZE_COLUMN_LABEL'), 110, isJellyEnabled() ? false : true);
    // @ts-ignore: error TS2339: Property 'renderSize_' does not exist on type
    // 'Element'.
    sizeColumn.renderFunction = self.renderSize_.bind(self);
    sizeColumn.defaultOrder = 'desc';
    sizeColumn.headerRenderFunction = renderHeader_;

    const typeColumn =
        new TableColumn('type', str('TYPE_COLUMN_LABEL'), fullPage ? 110 : 110);
    // @ts-ignore: error TS2339: Property 'renderType_' does not exist on type
    // 'Element'.
    typeColumn.renderFunction = self.renderType_.bind(self);
    typeColumn.headerRenderFunction = renderHeader_;

    const modTimeColumn = new TableColumn(
        'modificationTime', str('DATE_COLUMN_LABEL'), fullPage ? 150 : 210);
    // @ts-ignore: error TS2339: Property 'renderDate_' does not exist on type
    // 'Element'.
    modTimeColumn.renderFunction = self.renderDate_.bind(self);
    modTimeColumn.defaultOrder = 'desc';
    modTimeColumn.headerRenderFunction = renderHeader_;

    const columns = [nameColumn, sizeColumn, typeColumn, modTimeColumn];

    const columnModel = new FileTableColumnModel(columns);

    // @ts-ignore: error TS2339: Property 'columnModel' does not exist on type
    // 'Element'.
    self.columnModel = columnModel;

    // @ts-ignore: error TS2339: Property 'formatter_' does not exist on type
    // 'Element'.
    self.formatter_ = new FileMetadataFormatter();

    // @ts-ignore: error TS2352: Conversion of type 'Element' to type 'Table'
    // may be a mistake because neither type sufficiently overlaps with the
    // other. If this was intentional, convert the expression to 'unknown'
    // first.
    const selfAsTable = /** @type {!Table} */ (self);
    selfAsTable.setRenderFunction(
        // @ts-ignore: error TS2339: Property 'renderTableRow_' does not exist
        // on type 'Element'.
        self.renderTableRow_.bind(self, selfAsTable.getRenderFunction()));

    // Keep focus on the file list when clicking on the header.
    selfAsTable.header.addEventListener('mousedown', e => {
      // @ts-ignore: error TS2339: Property 'list' does not exist on type
      // 'Element'.
      self.list.focus();
      e.preventDefault();
    });

    // @ts-ignore: error TS2339: Property 'relayoutRateLimiter_' does not exist
    // on type 'Element'.
    self.relayoutRateLimiter_ =
        // @ts-ignore: error TS2339: Property 'relayoutImmediately_' does not
        // exist on type 'Element'.
        new RateLimiter(self.relayoutImmediately_.bind(self));

    // Save the last selection. This is used by shouldStartDragSelection.
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    self.list.addEventListener('mousedown', function(e) {
      // @ts-ignore: error TS2339: Property 'selectionModel' does not exist on
      // type '(Anonymous function)'.
      this.lastSelection_ = this.selectionModel.selectedIndexes;
    }.bind(self), true);
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    self.list.addEventListener('touchstart', function(e) {
      // @ts-ignore: error TS2339: Property 'selectionModel' does not exist on
      // type '(Anonymous function)'.
      this.lastSelection_ = this.selectionModel.selectedIndexes;
    }.bind(self), true);
    // @ts-ignore: error TS2339: Property 'list' does not exist on type
    // 'Element'.
    self.list.shouldStartDragSelection =
        // @ts-ignore: error TS2339: Property 'shouldStartDragSelection_' does
        // not exist on type 'Element'.
        self.shouldStartDragSelection_.bind(self);

    // @ts-ignore: error TS2339: Property 'list' does not exist on type
    // 'Element'.
    self.list.addEventListener(
        // @ts-ignore: error TS2339: Property 'onMouseOver_' does not exist on
        // type 'Element'.
        'mouseover', self.onMouseOver_.bind(self), {passive: true});

    // Update the item's inline status when it's restored from List's cache.
    // @ts-ignore: error TS2339: Property 'list' does not exist on type
    // 'Element'.
    self.list.addEventListener(
        'cachedItemRestored',
        // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
        (e) => filelist.updateCacheItemInlineStatus(
            // @ts-ignore: error TS2339: Property 'metadataModel_' does not
            // exist on type 'Element'.
            e.detail, self.dataModel, self.metadataModel_));
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onMouseOver_(event) {
    this.maybeShowToolTip(event);
  }

  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  maybeShowToolTip(event) {
    const target = event.composedPath()[0];
    if (!target) {
      return;
    }
    if (!target.classList.contains('detail-name')) {
      return;
    }
    const labelElement = target.querySelector('.filename-label');
    if (!labelElement) {
      return;
    }

    maybeShowTooltip(labelElement, labelElement.innerText);
  }

  /**
   * Sort data by the given column. Overridden to add the a11y message after
   * sorting.
   * @param {number} index The index of the column to sort by.
   * @override
   */
  sort(index) {
    const cm = this.columnModel;
    if (!this.dataModel) {
      return;
    }
    const fieldName = cm.getId(index);
    const sortStatus = this.dataModel.sortStatus;

    let sortDirection = cm.getDefaultOrder(index);
    // @ts-ignore: error TS2339: Property 'field' does not exist on type
    // 'Object'.
    if (sortStatus.field === fieldName) {
      // If it's sorting the column that's already sorted, we need to flip the
      // sorting order.
      // @ts-ignore: error TS2339: Property 'direction' does not exist on type
      // 'Object'.
      sortDirection = sortStatus.direction === 'desc' ? 'asc' : 'desc';
    }

    const msgId =
        sortDirection === 'asc' ? 'COLUMN_SORTED_ASC' : 'COLUMN_SORTED_DESC';
    const msg = strf(msgId, fieldName);

    // Delegate to parent to sort.
    super.sort(index);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.a11y.speakA11yMessage(msg);
  }

  /**
   * @override
   */
  onDataModelSorted() {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const hasGroupHeadingAfterSort = fileListModel.shouldShowGroupHeading();
    // Sort doesn't trigger redraw sometimes, e.g. if we sort by Name for now,
    // then we sort by time, if the list order doesn't change, no permuted event
    // is triggered, thus no redraw is triggered. In this scenario, we need to
    // manually trigger a redraw to remove/add the group heading.
    if (hasGroupHeadingAfterSort !== fileListModel.hasGroupHeadingBeforeSort) {
      // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
      // 'List'.
      this.list.redraw();
    }
  }

  /**
   * Updates high priority range of list thumbnail loader based on current
   * viewport.
   *
   * @param {number} beginIndex Begin index.
   * @param {number} endIndex End index.
   * @private
   */
  // @ts-ignore: error TS6133: 'updateHighPriorityRange_' is declared but its
  // value is never read.
  updateHighPriorityRange_(beginIndex, endIndex) {
    // Keep these values to set range when a new list thumbnail loader is set.
    this.beginIndex_ = beginIndex;
    this.endIndex_ = endIndex;

    if (this.listThumbnailLoader_ !== null) {
      this.listThumbnailLoader_.setHighPriorityRange(beginIndex, endIndex);
    }
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
    const container = listItem.querySelector('.detail-thumbnail');
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
  // @ts-ignore: error TS6133: 'onThumbnailLoaded_' is declared but its value is
  // never read.
  onThumbnailLoaded_(event) {
    // @ts-ignore: error TS2339: Property 'index' does not exist on type
    // 'Event'.
    const listItem = this.getListItemByIndex(event.index);
    if (listItem) {
      const box = listItem.querySelector('.detail-thumbnail');
      if (box) {
        // @ts-ignore: error TS2339: Property 'dataUrl' does not exist on type
        // 'Event'.
        if (event.dataUrl) {
          this.setThumbnailImage_(
              // @ts-ignore: error TS2339: Property 'dataUrl' does not exist on
              // type 'Event'.
              assertInstanceof(box, HTMLDivElement), event.dataUrl);
        } else {
          this.clearThumbnailImage_(assertInstanceof(box, HTMLDivElement));
        }
        const icon = listItem.querySelector('.detail-icon');
        // @ts-ignore: error TS2339: Property 'dataUrl' does not exist on type
        // 'Event'.
        icon.classList.toggle('has-thumbnail', !!event.dataUrl);
      }
    }
  }

  /**
   * Adjust column width to fit its content.
   * @param {number} index Index of the column to adjust width.
   * @override
   */
  fitColumn(index) {
    const render = this.columnModel.getRenderFunction(index);
    const MAXIMUM_ROWS_TO_MEASURE = 1000;

    // Create a temporaty list item, put all cells into it and measure its
    // width. Then remove the item. It fits "list > *" CSS rules.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'FileTable'.
    const container = this.ownerDocument.createElement('li');
    container.style.display = 'inline-block';
    container.style.textAlign = 'start';
    // The container will have width of the longest cell.
    container.style.webkitBoxOrient = 'vertical';

    // Select at most MAXIMUM_ROWS_TO_MEASURE items around visible area.
    // @ts-ignore: error TS2339: Property 'getItemsInViewPort' does not exist on
    // type 'List'.
    const items = this.list.getItemsInViewPort(
        this.list.scrollTop, this.list.clientHeight);
    const firstIndex = Math.floor(
        Math.max(0, (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
    const lastIndex =
        Math.min(this.dataModel.length, firstIndex + MAXIMUM_ROWS_TO_MEASURE);
    for (let i = firstIndex; i < lastIndex; i++) {
      const item = this.dataModel.item(i);
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type 'FileTable'.
      const div = this.ownerDocument.createElement('div');
      div.className = 'table-row-cell';
      // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
      // parameter of type 'Element'.
      div.appendChild(render(item, this.columnModel.getId(index), this));
      container.appendChild(div);
    }
    this.list.appendChild(container);
    const width = parseFloat(window.getComputedStyle(container).width);
    this.list.removeChild(container);

    // @ts-ignore: error TS2339: Property 'initializeColumnPos' does not exist
    // on type 'TableColumnModel'.
    this.columnModel.initializeColumnPos();
    // @ts-ignore: error TS2339: Property 'setWidthAndKeepTotal' does not exist
    // on type 'TableColumnModel'.
    this.columnModel.setWidthAndKeepTotal(index, Math.ceil(width));
    // @ts-ignore: error TS2339: Property 'destroyColumnPos' does not exist on
    // type 'TableColumnModel'.
    this.columnModel.destroyColumnPos();
  }

  /**
   * Sets date and time format.
   * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
   */
  setDateTimeFormat(use12hourClock) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.formatter_.setDateTimeFormat(use12hourClock);
  }

  /**
   * Sets whether to use modificationByMeTime as "Last Modified" time.
   * @param {boolean} useModificationByMeTime
   */
  setUseModificationByMeTime(useModificationByMeTime) {
    this.useModificationByMeTime_ = useModificationByMeTime;
  }

  /**
   * Obtains if the drag selection should be start or not by referring the mouse
   * event.
   * @param {MouseEvent} event Drag start event.
   * @return {boolean} True if the mouse is hit to the background of the list,
   * or certain areas of the inside of the list that would start a drag
   * selection.
   * @private
   */
  // @ts-ignore: error TS6133: 'shouldStartDragSelection_' is declared but its
  // value is never read.
  shouldStartDragSelection_(event) {
    // If the shift key is pressed, it should starts drag selection.
    if (event.shiftKey) {
      return true;
    }

    // If we're outside of the element list, start the drag selection.
    if (!/** @type {FileTableList} */ (this.list).hasDragHitElement(event)) {
      return true;
    }

    // If the position values are negative, it points the out of list.
    const pos = DragSelector.getScrolledPosition(this.list, event);
    if (!pos) {
      return false;
    }
    // @ts-ignore: error TS2339: Property 'y' does not exist on type 'Object'.
    if (pos.x < 0 || pos.y < 0) {
      return true;
    }

    // If the item index is out of range, it should start the drag selection.
    // @ts-ignore: error TS2339: Property 'measureItem' does not exist on type
    // 'List'.
    const itemHeight = this.list.measureItem().height;
    // Faster alternative to Math.floor for non-negative numbers.
    // @ts-ignore: error TS2339: Property 'y' does not exist on type 'Object'.
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

    // If the horizontal value is not hit to column, it should start the drag
    // selection.
    // @ts-ignore: error TS2339: Property 'x' does not exist on type 'Object'.
    const hitColumn = this.columnModel.getHitColumn(pos.x);
    if (!hitColumn) {
      return true;
    }

    // Check if the point is on the column contents or not.
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    switch (this.columnModel.columns_[hitColumn.index].id) {
      case 'name':
        const item = this.list.getListItemByIndex(itemIndex);
        if (!item) {
          return false;
        }

        const spanElement = item.querySelector('.filename-label span');
        const spanRect = spanElement && spanElement.getBoundingClientRect();
        // The this.list.cachedBounds_ object is set by
        // DragSelector.getScrolledPosition.
        // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist on
        // type 'List'.
        if (!this.list.cachedBounds) {
          return true;
        }
        const textRight =
            // @ts-ignore: error TS2339: Property 'cachedBounds' does not exist
            // on type 'List'.
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
   * @param {!Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {Table} table The table doing the rendering.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  // @ts-ignore: error TS6133: 'table' is declared but its value is never read.
  renderName_(entry, columnId, table) {
    const label = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const metadata = this.metadataModel_.getCache(
        [entry], ['contentMimeType', 'isDlpRestricted'])[0];
    // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
    const mimeType = metadata.contentMimeType;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const icon = filelist.renderFileTypeIcon(
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        this.ownerDocument, entry, locationInfo, mimeType);
    if (FileType.isImage(entry, mimeType) ||
        FileType.isVideo(entry, mimeType) ||
        FileType.isAudio(entry, mimeType) || FileType.isRaw(entry, mimeType)) {
      icon.appendChild(this.renderThumbnail_(entry, icon));
    }
    icon.appendChild(this.renderCheckmark_());
    label.appendChild(icon);
    if (isDriveShortcutsEnabled()) {
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type 'FileTable'.
      label.appendChild(filelist.renderIconBadge(this.ownerDocument));
    }
    // @ts-ignore: error TS2339: Property 'entry' does not exist on type
    // 'HTMLDivElement'.
    label.entry = entry;
    label.className = 'detail-name';
    label.appendChild(
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        filelist.renderFileNameLabel(this.ownerDocument, entry, locationInfo));
    if (locationInfo && locationInfo.isDriveBased) {
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type 'FileTable'.
      const inlineStatus = this.ownerDocument.createElement('xf-inline-status');
      inlineStatus.classList.add('tast-inline-status');
      label.appendChild(inlineStatus);
    }
    if (!isJellyEnabled() && !isInlineSyncStatusEnabled()) {
      // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
      const isEncrypted = FileType.isEncrypted(entry, metadata.contentMimeType);
      if (isEncrypted) {
        label.appendChild(this.renderEncryptedIcon_());
      }
      if (isDlpEnabled()) {
        label.appendChild(
            // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
            this.renderDlpManagedIcon_(!!metadata.isDlpRestricted));
      }
    }
    return label;
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

    const locationInfo = this.volumeManager_?.getLocationInfo(entry) || null;
    return getEntryLabel(locationInfo, entry);
  }

  /**
   * Render the Size column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {Table} table The table doing the rendering.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  // @ts-ignore: error TS6133: 'table' is declared but its value is never read.
  renderSize_(entry, columnId, table) {
    const div = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
    div.className = 'size';
    this.updateSize_(div, entry);

    return div;
  }

  /**
   * Sets up or updates the size cell.
   *
   * @param {HTMLDivElement} div The table cell.
   * @param {Entry} entry The corresponding entry.
   * @private
   */
  updateSize_(div, entry) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const metadata = this.metadataModel_.getCache(
        [entry], ['size', 'hosted', 'contentMimeType'])[0];
    // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
    const size = metadata.size;
    // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
    const special = metadata.hosted ||
        // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
        FileType.isEncrypted(entry, metadata.contentMimeType);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    div.textContent = this.formatter_.formatSize(size, special);
  }

  /**
   * Render the Type column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {Table} table The table doing the rendering.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  // @ts-ignore: error TS6133: 'table' is declared but its value is never read.
  renderType_(entry, columnId, table) {
    const div = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
    div.className = 'type';

    const mimeType =
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        this.metadataModel_.getCache([entry], ['contentMimeType'])[0]
            .contentMimeType;
    div.textContent =
        FileListModel.getFileTypeString(FileType.getType(entry, mimeType));
    return div;
  }

  /**
   * Render the Date column of the detail table.
   *
   * @param {Entry} entry The Entry object to render.
   * @param {string} columnId The id of the column to be rendered.
   * @param {Table} table The table doing the rendering.
   * @return {HTMLDivElement} Created element.
   * @private
   */
  // @ts-ignore: error TS6133: 'table' is declared but its value is never read.
  renderDate_(entry, columnId, table) {
    const div = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));

    if (isJellyEnabled() || isInlineSyncStatusEnabled()) {
      div.className = 'dateholder';
      const label = /** @type {!HTMLDivElement} */
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type 'FileTable'.
          (this.ownerDocument.createElement('div'));
      div.appendChild(label);
      label.className = 'date';
      this.updateDate_(label, entry);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const metadata = this.metadataModel_.getCache(
          [entry], ['contentMimeType', 'isDlpRestricted'])[0];
      // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
      const isEncrypted = FileType.isEncrypted(entry, metadata.contentMimeType);
      if (isEncrypted) {
        div.appendChild(this.renderEncryptedIcon_());
      }
      if (isDlpEnabled()) {
        // @ts-ignore: error TS18048: 'metadata' is possibly 'undefined'.
        div.appendChild(this.renderDlpManagedIcon_(!!metadata.isDlpRestricted));
      }
    } else {
      div.className = 'date';
      this.updateDate_(div, entry);
    }
    return div;
  }

  /**
   * Sets up or updates the date cell.
   *
   * @param {HTMLDivElement} div The table cell.
   * @param {Entry} entry Entry of file to update.
   * @private
   */
  updateDate_(div, entry) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const item = this.metadataModel_.getCache(
        [entry], ['modificationTime', 'modificationByMeTime'])[0];
    const modTime = this.useModificationByMeTime_ ?
        // @ts-ignore: error TS18048: 'item' is possibly 'undefined'.
        item.modificationByMeTime || item.modificationTime :
        // @ts-ignore: error TS18048: 'item' is possibly 'undefined'.
        item.modificationTime;

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    div.textContent = this.formatter_.formatModDate(modTime);
  }

  updateGroupHeading_() {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    if (fileListModel &&
        fileListModel.groupByField === GROUP_BY_FIELD_MODIFICATION_TIME) {
      // TODO(crbug.com/1353650): find a way to update heading instead of redraw
      this.redraw();
    }
  }

  /**
   * Updates the file metadata in the table item.
   *
   * @param {Element} item Table item.
   * @param {Entry} entry File entry.
   */
  updateFileMetadata(item, entry) {
    this.updateDate_(
        /** @type {!HTMLDivElement} */ (item.querySelector('.date')), entry);
    this.updateSize_(
        /** @type {!HTMLDivElement} */ (item.querySelector('.size')), entry);
  }

  /**
   * Updates list items 'in place' on metadata change.
   * @param {string} type Type of metadata change.
   * @param {Array<Entry>} entries Entries to update.
   */
  updateListItemsMetadata(type, entries) {
    const urls = entriesToURLs(entries);
    // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
    // type.
    const forEachCell = (selector, callback) => {
      // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist on
      // type 'FileTable'.
      const cells = this.querySelectorAll(selector);
      for (let i = 0; i < cells.length; i++) {
        const cell = /** @type {HTMLElement} */ (cells[i]);
        // @ts-ignore: error TS2551: Property 'list_' does not exist on type
        // 'FileTable'. Did you mean 'list'?
        const listItem = this.list_.getListItemAncestor(cell);
        const index = listItem?.listIndex ?? 0;
        const entry = this.dataModel.item(index);
        if (entry && urls.indexOf(entry.toURL()) !== -1) {
          callback.call(this, cell, entry, listItem);
        }
      }
    };
    if (type === 'filesystem') {
      // @ts-ignore: error TS7006: Parameter 'unused' implicitly has an 'any'
      // type.
      forEachCell('.table-row-cell .date', function(item, entry, unused) {
        // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
        // does not have a type annotation.
        this.updateDate_(item, entry);
      });
      // @ts-ignore: error TS7006: Parameter 'unused' implicitly has an 'any'
      // type.
      forEachCell('.table-row-cell > .size', function(item, entry, unused) {
        // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
        // does not have a type annotation.
        this.updateSize_(item, entry);
      });
      this.updateGroupHeading_();
    } else if (type === 'external') {
      // The cell name does not matter as the entire list item is needed.
      // @ts-ignore: error TS7006: Parameter 'listItem' implicitly has an 'any'
      // type.
      forEachCell('.table-row-cell .date', function(item, entry, listItem) {
        filelist.updateListItemExternalProps(
            listItem, entry,
            // @ts-ignore: error TS2683: 'this' implicitly has type 'any'
            // because it does not have a type annotation.
            this.metadataModel_.getCache(
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
                ])[0],
            isTeamDriveRoot(entry));
        listItem.toggleAttribute(
            'disabled',
            filelist.isDlpBlocked(
                // @ts-ignore: error TS2683: 'this' implicitly has type 'any'
                // because it does not have a type annotation.
                entry, assert(this.metadataModel_),
                // @ts-ignore: error TS2683: 'this' implicitly has type 'any'
                // because it does not have a type annotation.
                assert(this.volumeManager_)));
      });
    }
  }

  /**
   * Renders table row.
   * @param {function(Entry, Table):void} baseRenderFunction Base renderer.
   * @param {Entry} entry Corresponding entry.
   * @return {HTMLLIElement} Created element.
   * @private
   */
  // @ts-ignore: error TS6133: 'renderTableRow_' is declared but its value is
  // never read.
  renderTableRow_(baseRenderFunction, entry) {
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'Table'.
    const item = baseRenderFunction(entry, this);
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const nameId = item.id + '-entry-name';
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const sizeId = item.id + '-size';
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const typeId = item.id + '-type';
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const dateId = item.id + '-date';
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const dlpId = item.id + '-dlp-managed-icon';
    // @ts-ignore: error TS2339: Property 'id' does not exist on type 'void'.
    const encryptedId = item.id + '-encrypted-icon';
    filelist.decorateListItem(
        // @ts-ignore: error TS2345: Argument of type 'void' is not assignable
        // to parameter of type 'ListItem'.
        item, entry, assert(this.metadataModel_), assert(this.volumeManager_));
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'void'.
    item.setAttribute('file-name', entry.name);
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    item.querySelector('.detail-name').setAttribute('id', nameId);
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    item.querySelector('.size').setAttribute('id', sizeId);
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    item.querySelector('.type').setAttribute('id', typeId);
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    item.querySelector('.date').setAttribute('id', dateId);
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    const dlpManagedIcon = item.querySelector('.dlp-managed-icon');
    if (dlpManagedIcon) {
      dlpManagedIcon.setAttribute('id', dlpId);
      // @ts-ignore: error TS2304: Cannot find name 'FilesTooltip'.
      /** @type {!FilesTooltip} */ (
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type 'FileTable'.
          this.ownerDocument.querySelector('files-tooltip'))
          // @ts-ignore: error TS2339: Property 'querySelectorAll' does not
          // exist on type 'void'.
          .addTargets(item.querySelectorAll('.dlp-managed-icon'));
    }
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'void'.
    const encryptedIcon = item.querySelector('.encrypted-icon');
    if (encryptedIcon) {
      encryptedIcon.setAttribute('id', encryptedId);
    }

    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // 'void'.
    item.setAttribute(
        'aria-labelledby',
        `${nameId} column-size ${sizeId} column-type ${
            typeId} column-modificationTime ${dateId} ${encryptedId}`);
    // @ts-ignore: error TS2322: Type 'void' is not assignable to type
    // 'HTMLLIElement'.
    return item;
  }

  /**
   * Renders the file thumbnail in the detail table.
   * @param {Entry} entry The Entry object to render.
   * @param {HTMLDivElement} parent The parent DOM element.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  renderThumbnail_(entry, parent) {
    const box = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
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
   * @param {!HTMLDivElement} box Detail thumbnail div element.
   * @param {string} dataUrl Data url of thumbnail.
   * @private
   */
  setThumbnailImage_(box, dataUrl) {
    const thumbnail = box.ownerDocument.createElement('div');
    thumbnail.classList.add('thumbnail');
    thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';
    const oldThumbnails = box.querySelectorAll('.thumbnail');

    for (let i = 0; i < oldThumbnails.length; i++) {
      // @ts-ignore: error TS2345: Argument of type 'Element | undefined' is not
      // assignable to parameter of type 'Node'.
      box.removeChild(oldThumbnails[i]);
    }

    box.appendChild(thumbnail);
  }

  /**
   * Clears thumbnail image from the box.
   * @param {!HTMLDivElement} box Detail thumbnail div element.
   * @private
   */
  clearThumbnailImage_(box) {
    const oldThumbnails = box.querySelectorAll('.thumbnail');

    for (let i = 0; i < oldThumbnails.length; i++) {
      // @ts-ignore: error TS2345: Argument of type 'Element | undefined' is not
      // assignable to parameter of type 'Node'.
      box.removeChild(oldThumbnails[i]);
    }
  }

  /**
   * Renders the selection checkmark in the detail table.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  renderCheckmark_() {
    const checkmark = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
    checkmark.className = 'detail-checkmark';
    return checkmark;
  }

  /**
   * Renders the DLP managed icon in the detail table.
   * @param {!boolean} isDlpRestricted Whether the icon should be shown.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  renderDlpManagedIcon_(isDlpRestricted) {
    const icon = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
    icon.className = 'dlp-managed-icon';
    icon.toggleAttribute('has-tooltip');
    icon.dataset['tooltipLinkHref'] = str('DLP_HELP_URL');
    icon.dataset['tooltipLinkAriaLabel'] = str('DLP_MANAGED_ICON_TOOLTIP_DESC');
    icon.dataset['tooltipLinkText'] = str('DLP_MANAGED_ICON_TOOLTIP_LINK');
    icon.setAttribute('aria-label', str('DLP_MANAGED_ICON_TOOLTIP'));
    icon.toggleAttribute('show-card-tooltip');
    icon.classList.toggle('is-dlp-restricted', isDlpRestricted);
    icon.toggleAttribute('aria-hidden', isDlpRestricted);
    return icon;
  }

  /**
   * Renders the encrypted icon in the detail table, used to mark Google Drive
   * CSE files.
   * @return {!HTMLDivElement} Created element.
   * @private
   */
  renderEncryptedIcon_() {
    const icon = /** @type {!HTMLDivElement} */
        // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
        // type 'FileTable'.
        (this.ownerDocument.createElement('div'));
    icon.className = 'encrypted-icon';
    icon.setAttribute('aria-label', str('ENCRYPTED_ICON_TOOLTIP'));
    // @ts-ignore: error TS2304: Cannot find name 'FilesTooltip'.
    /** @type {!FilesTooltip} */ (document.querySelector('files-tooltip'))
        .addTarget(icon);
    return icon;
  }

  /**
   * Redraws the UI. Skips multiple consecutive calls.
   */
  relayout() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.relayoutRateLimiter_.run();
  }

  /**
   * Redraws the UI immediately.
   * @private
   */
  // @ts-ignore: error TS6133: 'relayoutImmediately_' is declared but its value
  // is never read.
  relayoutImmediately_() {
    // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on type
    // 'FileTable'.
    if (this.clientWidth > 0) {
      this.normalizeColumns();
    }
    this.redraw();
    dispatchSimpleEvent(this.list, 'relayout');
  }
}

/**
 * Inherits from Table.
 */
FileTable.prototype.__proto__ = Table.prototype;
