// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom column model for advanced auto-resizing.
 *
 * @param {!Array<cr.ui.table.TableColumn>} tableColumns Table columns.
 * @extends {cr.ui.table.TableColumnModel}
 * @constructor
 */
function FileTableColumnModel(tableColumns) {
  cr.ui.table.TableColumnModel.call(this, tableColumns);
}

/**
 * Inherits from cr.ui.TableColumnModel.
 */
FileTableColumnModel.prototype.__proto__ =
    cr.ui.table.TableColumnModel.prototype;

/**
 * Minimum width of column. Note that is not marked private as it is used in the
 * unit tests.
 * @const {number}
 */
FileTableColumnModel.MIN_WIDTH_ = 10;

/**
 * Sets column width so that the column dividers move to the specified position.
 * This function also check the width of each column and keep the width larger
 * than MIN_WIDTH_.
 *
 * @private
 * @param {Array<number>} newPos Positions of each column dividers.
 */
FileTableColumnModel.prototype.applyColumnPositions_ = function(newPos) {
  // Check the minimum width and adjust the positions.
  for (var i = 0; i < newPos.length - 2; i++) {
    if (!this.columns_[i].visible) {
      newPos[i + 1] = newPos[i];
    } else if (newPos[i + 1] - newPos[i] < FileTableColumnModel.MIN_WIDTH_) {
      newPos[i + 1] = newPos[i] + FileTableColumnModel.MIN_WIDTH_;
    }
  }
  for (var i = newPos.length - 1; i >= 2; i--) {
    if (!this.columns_[i - 1].visible) {
      newPos[i - 1] = newPos[i];
    } else if (newPos[i] - newPos[i - 1] < FileTableColumnModel.MIN_WIDTH_) {
      newPos[i - 1] = newPos[i] - FileTableColumnModel.MIN_WIDTH_;
    }
  }
  // Set the new width of columns
  for (var i = 0; i < this.columns_.length; i++) {
    if (!this.columns_[i].visible) {
      this.columns_[i].width = 0;
    } else {
      // Make sure each cell has the minumum width. This is necessary when the
      // window size is too small to contain all the columns.
      this.columns_[i].width = Math.max(FileTableColumnModel.MIN_WIDTH_,
                                        newPos[i + 1] - newPos[i]);
    }
  }
};

/**
 * Normalizes widths to make their sum 100% if possible. Uses the proportional
 * approach with some additional constraints.
 *
 * @param {number} contentWidth Target width.
 * @override
 */
FileTableColumnModel.prototype.normalizeWidths = function(contentWidth) {
  var totalWidth = 0;
  // Some columns have fixed width.
  for (var i = 0; i < this.columns_.length; i++) {
    totalWidth += this.columns_[i].width;
  }
  var positions = [0];
  var sum = 0;
  for (var i = 0; i < this.columns_.length; i++) {
    var column = this.columns_[i];
    sum += column.width;
    // Faster alternative to Math.floor for non-negative numbers.
    positions[i + 1] = ~~(contentWidth * sum / totalWidth);
  }
  this.applyColumnPositions_(positions);
};

/**
 * Handles to the start of column resizing by splitters.
 */
FileTableColumnModel.prototype.handleSplitterDragStart = function() {
  this.initializeColumnPos();
};

/**
 * Handles to the end of column resizing by splitters.
 */
FileTableColumnModel.prototype.handleSplitterDragEnd = function() {
  this.destroyColumnPos();
};

/**
 * Initialize a column snapshot which is used in setWidthAndKeepTotal().
 */
FileTableColumnModel.prototype.initializeColumnPos = function() {
  this.snapshot_ = new FileTableColumnModel.ColumnSnapshot(this.columns_);
};

/**
 * Destroy the column snapshot which is used in setWidthAndKeepTotal().
 */
FileTableColumnModel.prototype.destroyColumnPos = function() {
  this.snapshot_ = null;
};

/**
 * Sets the width of column while keeping the total width of table.
 * Before and after calling this method, you must initialize and destroy
 * columnPos with initializeColumnPos() and destroyColumnPos().
 * @param {number} columnIndex Index of column that is resized.
 * @param {number} columnWidth New width of the column.
 */
FileTableColumnModel.prototype.setWidthAndKeepTotal = function(
    columnIndex, columnWidth) {
  columnWidth = Math.max(columnWidth, FileTableColumnModel.MIN_WIDTH_);
  this.snapshot_.setWidth(columnIndex, columnWidth);
  this.applyColumnPositions_(this.snapshot_.newPos);

  // Notify about resizing
  cr.dispatchSimpleEvent(this, 'resize');
};

/**
 * Obtains a column by the specified horizontal position.
 * @param {number} x Horizontal position.
 * @return {Object} The object that contains column index, column width, and
 *     hitPosition where the horizontal position is hit in the column.
 */
FileTableColumnModel.prototype.getHitColumn = function(x) {
  for (var i = 0; x >= this.columns_[i].width; i++) {
    x -= this.columns_[i].width;
  }
  if (i >= this.columns_.length)
    return null;
  return {index: i, hitPosition: x, width: this.columns_[i].width};
};

/** @override */
FileTableColumnModel.prototype.setVisible = function(index, visible) {
  if (index < 0 || index > this.columns_.length - 1)
    return;

  var column = this.columns_[index];
  if (column.visible === visible)
    return;

  // Re-layout the table.  This overrides the default column layout code in the
  // parent class.
  var snapshot = new FileTableColumnModel.ColumnSnapshot(this.columns_);

  column.visible = visible;

  // Keep the current column width, but adjust the other columns to accomodate
  // the new column.
  snapshot.setWidth(index, column.width);
  this.applyColumnPositions_(snapshot.newPos);
};

/**
 * Export a set of column widths for use by #restoreColumnWidths.  Use these two
 * methods instead of manually saving and setting column widths, because doing
 * the latter will not correctly save/restore column widths for hidden columns.
 * @see #restoreColumnWidths
 * @return {!Object} config
 */
FileTableColumnModel.prototype.exportColumnConfig = function() {
  // Make a snapshot, and use that to compute a column layout where all the
  // columns are visible.
  var snapshot = new FileTableColumnModel.ColumnSnapshot(this.columns_);
  for (var i = 0; i < this.columns_.length; i++) {
    if (!this.columns_[i].visible) {
      snapshot.setWidth(i, this.columns_[i].absoluteWidth);
    }
  }
  // Export the column widths.
  var config = {};
  for (var i = 0; i < this.columns_.length; i++) {
    config[this.columns_[i].id] = {
      width: snapshot.newPos[i + 1] - snapshot.newPos[i]
    };
  }
  return config;
};

/**
 * Restores a set of column widths previously created by calling
 * #exportColumnConfig.
 * @see #exportColumnConfig
 * @param {!Object} config
 */
FileTableColumnModel.prototype.restoreColumnConfig = function(config) {
  // Convert old-style raw column widths into new-style config objects.
  if (Array.isArray(config)) {
    var tmpConfig = {};
    tmpConfig[this.columns_[0].id] = config[0];
    tmpConfig[this.columns_[1].id] = config[1];
    tmpConfig[this.columns_[3].id] = config[2];
    tmpConfig[this.columns_[4].id] = config[3];
    config = tmpConfig;
  }

  // Columns must all be made visible before restoring their widths.  Save the
  // current visibility so it can be restored after.
  var visibility = [];
  for (var i = 0; i < this.columns_.length; i++) {
    visibility[i] = this.columns_[i].visible;
    this.columns_[i].visible = true;
  }

  // Do not use external setters (e.g. #setVisible, #setWidth) here because they
  // trigger layout thrash, and also try to dynamically resize columns, which
  // interferes with restoring the old column layout.
  for (var columnId in config) {
    var column = this.columns_[this.indexOf(columnId)];
    if (column) {
      // Set column width.  Ignore invalid widths.
      var width = ~~config[columnId].width;
      if (width > 0)
        column.width = width;
    }
  }

  // Restore column visibility.  Use setVisible here, to trigger table relayout.
  for (var i = 0; i < this.columns_.length; i++) {
    this.setVisible(i, visibility[i]);
  }
};

/**
 * A helper class for performing resizing of columns.
 * @param {!Array<!cr.ui.table.TableColumn>} columns
 * @constructor
 */
FileTableColumnModel.ColumnSnapshot = function(columns) {
  /** @private {!Array<number>} */
  this.columnPos_ = [0];
  for (var i = 0; i < columns.length; i++) {
    this.columnPos_[i + 1] = columns[i].width + this.columnPos_[i];
  }

  /**
   * Starts off as a copy of the current column positions, but gets modified.
   * @private {!Array<number>}
   */
  this.newPos = this.columnPos_.slice(0);
};

/**
 * Set the width of the given column.  The snapshot will keep the total width of
 * the table constant.
 * @param {number} index
 * @param {number} width
 */
FileTableColumnModel.ColumnSnapshot.prototype.setWidth = function(
    index, width) {
  // Skip to resize 'selection' column
  if (index < 0 ||
      index >= this.columnPos_.length - 1 ||
      !this.columnPos_) {
    return;
  }

  // Round up if the column is shrinking, and down if the column is expanding.
  // This prevents off-by-one drift.
  var currentWidth = this.columnPos_[index + 1] - this.columnPos_[index];
  var round = width < currentWidth ? Math.ceil : Math.floor;

  // Calculate new positions of column splitters.
  var newPosStart = this.columnPos_[index] + width;
  var posEnd = this.columnPos_[this.columnPos_.length - 1];
  for (var i = 0; i < index + 1; i++) {
    this.newPos[i] = this.columnPos_[i];
  }
  for (var i = index + 1; i < this.columnPos_.length - 1; i++) {
    var posStart = this.columnPos_[index + 1];
    this.newPos[i] = (posEnd - newPosStart) *
                (this.columnPos_[i] - posStart) /
                (posEnd - posStart) +
                newPosStart;
    this.newPos[i] = round(this.newPos[i]);
  }
  this.newPos[index] = this.columnPos_[index];
  this.newPos[this.columnPos_.length - 1] = posEnd;
};

/**
 * Custom splitter that resizes column with retaining the sum of all the column
 * width.
 */
var FileTableSplitter = cr.ui.define('div');

/**
 * Inherits from cr.ui.TableSplitter.
 */
FileTableSplitter.prototype.__proto__ = cr.ui.TableSplitter.prototype;

/**
 * Handles the drag start event.
 */
FileTableSplitter.prototype.handleSplitterDragStart = function() {
  cr.ui.TableSplitter.prototype.handleSplitterDragStart.call(this);
  this.table_.columnModel.handleSplitterDragStart();
};

/**
 * Handles the drag move event.
 * @param {number} deltaX Horizontal mouse move offset.
 */
FileTableSplitter.prototype.handleSplitterDragMove = function(deltaX) {
  this.table_.columnModel.setWidthAndKeepTotal(this.columnIndex,
                                               this.columnWidth_ + deltaX,
                                               true);
};

/**
 * Handles the drag end event.
 */
FileTableSplitter.prototype.handleSplitterDragEnd = function() {
  cr.ui.TableSplitter.prototype.handleSplitterDragEnd.call(this);
  this.table_.columnModel.handleSplitterDragEnd();
};

/**
 * File list Table View.
 * @constructor
 * @extends {cr.ui.Table}
 */
function FileTable() {
  throw new Error('Designed to decorate elements');
}

/**
 * Inherits from cr.ui.Table.
 */
FileTable.prototype.__proto__ = cr.ui.Table.prototype;

/**
 * Decorates the element.
 * @param {!Element} self Table to decorate.
 * @param {!MetadataModel} metadataModel To retrieve
 *     metadata.
 * @param {!VolumeManager} volumeManager To retrieve volume info.
 * @param {!importer.HistoryLoader} historyLoader
 * @param {boolean} fullPage True if it's full page File Manager,
 *                           False if a file open/save dialog.
 */
FileTable.decorate = function(
    self, metadataModel, volumeManager, historyLoader, fullPage) {
  cr.ui.Table.decorate(self);
  self.__proto__ = FileTable.prototype;
  FileTableList.decorate(self.list);
  self.list.setOnMergeItems(self.updateHighPriorityRange_.bind(self));
  self.metadataModel_ = metadataModel;
  self.volumeManager_ = volumeManager;
  self.historyLoader_ = historyLoader;

  /** @private {ListThumbnailLoader} */
  self.listThumbnailLoader_ = null;

  /** @private {number} */
  self.beginIndex_ = 0;

  /** @private {number} */
  self.endIndex_ = 0;

  /** @private {function(!Event)} */
  self.onThumbnailLoadedBound_ = self.onThumbnailLoaded_.bind(self);

  /**
   * Reflects the visibility of import status in the UI.  Assumption: import
   * status is only enabled in import-eligible locations.  See
   * ImportController#onDirectoryChanged.  For this reason, the code in this
   * class checks if import status is visible, and if so, assumes that all the
   * files are in an import-eligible location.
   * TODO(kenobi): Clean this up once import status is queryable from metadata.
   *
   * @private {boolean}
   */
  self.importStatusVisible_ = true;

  /** @private {boolean} */
  self.useModificationByMeTime_ = false;

  var nameColumn = new cr.ui.table.TableColumn(
      'name', str('NAME_COLUMN_LABEL'), fullPage ? 386 : 324);
  nameColumn.renderFunction = self.renderName_.bind(self);

  var sizeColumn = new cr.ui.table.TableColumn(
      'size', str('SIZE_COLUMN_LABEL'), 110, true);
  sizeColumn.renderFunction = self.renderSize_.bind(self);
  sizeColumn.defaultOrder = 'desc';

  var statusColumn = new cr.ui.table.TableColumn(
      'status', str('STATUS_COLUMN_LABEL'), 60, true);
  statusColumn.renderFunction = self.renderStatus_.bind(self);
  statusColumn.visible = self.importStatusVisible_;

  var typeColumn = new cr.ui.table.TableColumn(
      'type', str('TYPE_COLUMN_LABEL'), fullPage ? 110 : 110);
  typeColumn.renderFunction = self.renderType_.bind(self);

  var modTimeColumn = new cr.ui.table.TableColumn(
      'modificationTime', str('DATE_COLUMN_LABEL'), fullPage ? 150 : 210);
  modTimeColumn.renderFunction = self.renderDate_.bind(self);
  modTimeColumn.defaultOrder = 'desc';

  var columns = [
      nameColumn,
      sizeColumn,
      statusColumn,
      typeColumn,
      modTimeColumn
  ];

  var columnModel = new FileTableColumnModel(columns);

  self.columnModel = columnModel;

  self.formatter_ = new FileMetadataFormatter();

  var selfAsTable = /** @type {!cr.ui.Table} */ (self);
  selfAsTable.setRenderFunction(
      self.renderTableRow_.bind(self, selfAsTable.getRenderFunction()));

  // Keep focus on the file list when clicking on the header.
  selfAsTable.header.addEventListener('mousedown', function(e) {
    self.list.focus();
    e.preventDefault();
  });

  self.relayoutRateLimiter_ =
      new AsyncUtil.RateLimiter(self.relayoutImmediately_.bind(self));

  // Override header#redraw to use FileTableSplitter.
  /** @this {cr.ui.table.TableHeader} */
  selfAsTable.header.redraw = function() {
    this.__proto__.redraw.call(this);
    // Extend table splitters
    var splitters = this.querySelectorAll('.table-header-splitter');
    for (var i = 0; i < splitters.length; i++) {
      if (splitters[i] instanceof FileTableSplitter)
        continue;
      FileTableSplitter.decorate(splitters[i]);
    }
  };

  // Save the last selection. This is used by shouldStartDragSelection.
  self.list.addEventListener('mousedown', function(e) {
    this.lastSelection_ = this.selectionModel.selectedIndexes;
  }.bind(self), true);
  self.list.addEventListener('touchstart', function(e) {
    this.lastSelection_ = this.selectionModel.selectedIndexes;
  }.bind(self), true);
  self.list.shouldStartDragSelection =
      self.shouldStartDragSelection_.bind(self);
  self.list.hasDragHitElement = self.hasDragHitElement_.bind(self);

  /**
   * Obtains the index list of elements that are hit by the point or the
   * rectangle.
   *
   * @param {number} x X coordinate value.
   * @param {number} y Y coordinate value.
   * @param {number=} opt_width Width of the coordinate.
   * @param {number=} opt_height Height of the coordinate.
   * @return {Array<number>} Index list of hit elements.
   * @this {cr.ui.List}
   */
  self.list.getHitElements = function(x, y, opt_width, opt_height) {
    var currentSelection = [];
    var bottom = y + (opt_height || 0);
    for (var i = 0; i < this.selectionModel_.length; i++) {
      var itemMetrics = this.getHeightsForIndex(i);
      if (itemMetrics.top < bottom && itemMetrics.top + itemMetrics.height >= y)
        currentSelection.push(i);
    }
    return currentSelection;
  };
};

/**
 * Updates high priority range of list thumbnail loader based on current
 * viewport.
 *
 * @param {number} beginIndex Begin index.
 * @param {number} endIndex End index.
 * @private
 */
FileTable.prototype.updateHighPriorityRange_ = function(beginIndex, endIndex) {
  // Keep these values to set range when a new list thumbnail loader is set.
  this.beginIndex_ = beginIndex;
  this.endIndex_ = endIndex;

  if (this.listThumbnailLoader_ !== null)
    this.listThumbnailLoader_.setHighPriorityRange(beginIndex, endIndex);
};

/**
 * Sets list thumbnail loader.
 * @param {ListThumbnailLoader} listThumbnailLoader A list thumbnail loader.
 */
FileTable.prototype.setListThumbnailLoader = function(listThumbnailLoader) {
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
};

/**
 * Returns the element containing the thumbnail of a certain list item as
 * background image.
 * @param {number} index The index of the item containing the desired thumbnail.
 * @return {?Element} The element containing the thumbnail, or null, if an error
 *     occurred.
 */
FileTable.prototype.getThumbnail = function(index) {
  var listItem = this.getListItemByIndex(index);
  if (!listItem) {
    return null;
  }
  var container = listItem.querySelector('.detail-thumbnail');
  if (!container) {
    return null;
  }
  return container.querySelector('.thumbnail');
};

/**
 * Handles thumbnail loaded event.
 * @param {!Event} event An event.
 * @private
 */
FileTable.prototype.onThumbnailLoaded_ = function(event) {
  var listItem = this.getListItemByIndex(event.index);
  if (listItem) {
    var box = listItem.querySelector('.detail-thumbnail');
    if (box) {
      if (event.dataUrl) {
        this.setThumbnailImage_(
            assertInstanceof(box, HTMLDivElement), event.dataUrl,
            true /* with animation */);
      } else {
        this.clearThumbnailImage_(
            assertInstanceof(box, HTMLDivElement));
      }
    }
  }
};

/**
 * Adjust column width to fit its content.
 * @param {number} index Index of the column to adjust width.
 * @override
 */
FileTable.prototype.fitColumn = function(index) {
  var render = this.columnModel.getRenderFunction(index);
  var MAXIMUM_ROWS_TO_MEASURE = 1000;

  // Create a temporaty list item, put all cells into it and measure its
  // width. Then remove the item. It fits "list > *" CSS rules.
  var container = this.ownerDocument.createElement('li');
  container.style.display = 'inline-block';
  container.style.textAlign = 'start';
  // The container will have width of the longest cell.
  container.style.webkitBoxOrient = 'vertical';

  // Select at most MAXIMUM_ROWS_TO_MEASURE items around visible area.
  var items = this.list.getItemsInViewPort(this.list.scrollTop,
                                           this.list.clientHeight);
  var firstIndex = Math.floor(Math.max(0,
      (items.last + items.first - MAXIMUM_ROWS_TO_MEASURE) / 2));
  var lastIndex = Math.min(this.dataModel.length,
                           firstIndex + MAXIMUM_ROWS_TO_MEASURE);
  for (var i = firstIndex; i < lastIndex; i++) {
    var item = this.dataModel.item(i);
    var div = this.ownerDocument.createElement('div');
    div.className = 'table-row-cell';
    div.appendChild(render(item, this.columnModel.getId(index), this));
    container.appendChild(div);
  }
  this.list.appendChild(container);
  var width = parseFloat(window.getComputedStyle(container).width);
  this.list.removeChild(container);

  this.columnModel.initializeColumnPos();
  this.columnModel.setWidthAndKeepTotal(index, Math.ceil(width));
  this.columnModel.destroyColumnPos();
};

/**
 * Sets the visibility of the cloud import status column.
 * @param {boolean} visible
 */
FileTable.prototype.setImportStatusVisible = function(visible) {
  if (this.importStatusVisible_ != visible) {
    this.importStatusVisible_ = visible;
    this.columnModel.setVisible(this.columnModel.indexOf('status'), visible);
    this.relayout();
  }
};

/**
 * Sets date and time format.
 * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
 */
FileTable.prototype.setDateTimeFormat = function(use12hourClock) {
  this.formatter_.setDateTimeFormat(use12hourClock);
};

/**
 * Sets whether to use modificationByMeTime as "Last Modified" time.
 * @param {boolean} useModificationByMeTime
 */
FileTable.prototype.setUseModificationByMeTime = function(
    useModificationByMeTime) {
  this.useModificationByMeTime_ = useModificationByMeTime;
};

/**
 * Returns whether the drag event is inside a file entry in the list (and not
 * the background padding area).
 * @param {MouseEvent} event Drag start event.
 * @return {boolean} True if the mouse is over an element in the list, False if
 *                   it is in the background.
 */
FileTable.prototype.hasDragHitElement_ = function(event) {
  var pos = DragSelector.getScrolledPosition(this.list, event);
  return this.list.getHitElements(pos.x, pos.y).length !== 0;
};

/**
 * Obtains if the drag selection should be start or not by referring the mouse
 * event.
 * @param {MouseEvent} event Drag start event.
 * @return {boolean} True if the mouse is hit to the background of the list, or
 *                   certain areas of the inside of the list that would start a
 *                   drag selection.
 * @private
 */
FileTable.prototype.shouldStartDragSelection_ = function(event) {
  // If the shift key is pressed, it should starts drag selection.
  if (event.shiftKey)
    return true;

  // If we're outside of the element list, start the drag selection.
  if (!this.list.hasDragHitElement(event))
    return true;

  // If the position values are negative, it points the out of list.
  var pos = DragSelector.getScrolledPosition(this.list, event);
  if (!pos)
    return false;
  if (pos.x < 0 || pos.y < 0)
    return true;

  // If the item index is out of range, it should start the drag selection.
  var itemHeight = this.list.measureItem().height;
  // Faster alternative to Math.floor for non-negative numbers.
  var itemIndex = ~~(pos.y / itemHeight);
  if (itemIndex >= this.list.dataModel.length)
    return true;

  // If the pointed item is already selected, it should not start the drag
  // selection.
  if (this.lastSelection_ && this.lastSelection_.indexOf(itemIndex) !== -1)
    return false;

  // If the horizontal value is not hit to column, it should start the drag
  // selection.
  var hitColumn = this.columnModel.getHitColumn(pos.x);
  if (!hitColumn)
    return true;

  // Check if the point is on the column contents or not.
  switch (this.columnModel.columns_[hitColumn.index].id) {
    case 'name':
      var item = this.list.getListItemByIndex(itemIndex);
      if (!item)
        return false;

      var spanElement = item.querySelector('.filename-label span');
      var spanRect = spanElement.getBoundingClientRect();
      // The this.list.cachedBounds_ object is set by
      // DragSelector.getScrolledPosition.
      if (!this.list.cachedBounds)
        return true;
      var textRight =
          spanRect.left - this.list.cachedBounds.left + spanRect.width;
      return textRight <= hitColumn.hitPosition;
    default:
      return true;
  }
};

/**
 * Render the Name column of the detail table.
 *
 * Invoked by cr.ui.Table when a file needs to be rendered.
 *
 * @param {!Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderName_ = function(entry, columnId, table) {
  var label = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));

  var mimeType = this.metadataModel_.getCache([entry],
      ['contentMimeType'])[0].contentMimeType;
  var icon = filelist.renderFileTypeIcon(this.ownerDocument, entry, mimeType);
  if (FileType.isImage(entry, mimeType) || FileType.isVideo(entry, mimeType) ||
      FileType.isAudio(entry, mimeType) || FileType.isRaw(entry, mimeType)) {
    icon.appendChild(this.renderThumbnail_(entry));
  }
  icon.appendChild(this.renderCheckmark_());
  label.appendChild(icon);

  label.entry = entry;
  label.className = 'detail-name';
  label.appendChild(filelist.renderFileNameLabel(this.ownerDocument, entry));
  return label;
};

/**
 * Render the Size column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderSize_ = function(entry, columnId, table) {
  var div = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));
  div.className = 'size';
  this.updateSize_(div, entry);

  return div;
};

/**
 * Sets up or updates the size cell.
 *
 * @param {HTMLDivElement} div The table cell.
 * @param {Entry} entry The corresponding entry.
 * @private
 */
FileTable.prototype.updateSize_ = function(div, entry) {
  var metadata = this.metadataModel_.getCache(
      [entry], ['size', 'hosted'])[0];
  var size = metadata.size;
  var hosted = metadata.hosted;
  div.textContent = this.formatter_.formatSize(size, hosted);
};

/**
 * Render the Status column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderStatus_ = function(entry, columnId, table) {
  var div = /** @type {!HTMLDivElement} */ (
      this.ownerDocument.createElement('div'));
  div.className = 'status status-icon';
  if (entry) {
    this.updateStatus_(div, entry);
  }

  return div;
};

/**
 * Returns the status of the entry w.r.t. the given import destination.
 * @param {Entry} entry
 * @param {!importer.Destination} destination
 * @return {!Promise<string>} The import status - will be 'imported', 'copied',
 *     or 'unknown'.
 */
FileTable.prototype.getImportStatus_ = function(entry, destination) {
  // If import status is not visible, early out because there's no point
  // retrieving it.
  if (!this.importStatusVisible_ || !importer.isEligibleType(entry)) {
    // Our import history doesn't deal with directories.
    // TODO(kenobi): May need to revisit this if the above assumption changes.
    return Promise.resolve('unknown');
  }
  // For the compiler.
  var fileEntry = /** @type {!FileEntry} */ (entry);

  return this.historyLoader_.getHistory()
      .then(
          /** @param {!importer.ImportHistory} history */
          function(history) {
            return Promise.all([
                history.wasImported(fileEntry, destination),
                history.wasCopied(fileEntry, destination)
            ]);
          })
      .then(
          /** @param {!Array<boolean>} status */
          function(status) {
            if (status[0]) {
              return 'imported';
            } else if (status[1]) {
              return 'copied';
            } else {
              return 'unknown';
            }
          });
};

/**
 * Render the status icon of the detail table.
 *
 * @param {HTMLDivElement} div
 * @param {Entry} entry The Entry object to render.
 * @private
 */
FileTable.prototype.updateStatus_ = function(div, entry) {
  this.getImportStatus_(entry, importer.Destination.GOOGLE_DRIVE).then(
      /** @param {string} status */
      function(status) {
        div.setAttribute('file-status-icon', status);
      });
};

/**
 * Render the Type column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderType_ = function(entry, columnId, table) {
  var div = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));
  div.className = 'type';

  var mimeType = this.metadataModel_.getCache([entry],
      ['contentMimeType'])[0].contentMimeType;
  div.textContent = FileListModel.getFileTypeString(
      FileType.getType(entry, mimeType));
  return div;
};

/**
 * Render the Date column of the detail table.
 *
 * @param {Entry} entry The Entry object to render.
 * @param {string} columnId The id of the column to be rendered.
 * @param {cr.ui.Table} table The table doing the rendering.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderDate_ = function(entry, columnId, table) {
  var div = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));
  div.className = 'date';

  this.updateDate_(div, entry);
  return div;
};

/**
 * Sets up or updates the date cell.
 *
 * @param {HTMLDivElement} div The table cell.
 * @param {Entry} entry Entry of file to update.
 * @private
 */
FileTable.prototype.updateDate_ = function(div, entry) {
  // For now, Team Drive roots have the incorrect modified date value. Hide it
  // until we get the proper one (see https://crbug.com/861622).
  if (util.isTeamDriveRoot(entry)) {
    div.textContent = '--';
    return;
  }

  var item = this.metadataModel_.getCache(
      [entry], ['modificationTime', 'modificationByMeTime'])[0];
  var modTime = this.useModificationByMeTime_ ?
      item.modificationByMeTime || item.modificationTime :
      item.modificationTime;

  div.textContent = this.formatter_.formatModDate(modTime);
};

/**
 * Updates the file metadata in the table item.
 *
 * @param {Element} item Table item.
 * @param {Entry} entry File entry.
 */
FileTable.prototype.updateFileMetadata = function(item, entry) {
  this.updateDate_(
      /** @type {!HTMLDivElement} */ (item.querySelector('.date')), entry);
  this.updateSize_(
      /** @type {!HTMLDivElement} */ (item.querySelector('.size')), entry);
  this.updateStatus_(
      /** @type {!HTMLDivElement} */ (item.querySelector('.status')), entry);
};

/**
 * Updates list items 'in place' on metadata change.
 * @param {string} type Type of metadata change.
 * @param {Array<Entry>} entries Entries to update.
 */
FileTable.prototype.updateListItemsMetadata = function(type, entries) {
  var urls = util.entriesToURLs(entries);
  var forEachCell = function(selector, callback) {
    var cells = this.querySelectorAll(selector);
    for (var i = 0; i < cells.length; i++) {
      var cell = cells[i];
      var listItem = this.list_.getListItemAncestor(cell);
      var entry = this.dataModel.item(listItem.listIndex);
      if (entry && urls.indexOf(entry.toURL()) !== -1)
        callback.call(this, cell, entry, listItem);
    }
  }.bind(this);
  if (type === 'filesystem') {
    forEachCell('.table-row-cell > .date', function(item, entry, unused) {
      this.updateDate_(item, entry);
    });
    forEachCell('.table-row-cell > .size', function(item, entry, unused) {
      this.updateSize_(item, entry);
    });
  } else if (type === 'external') {
    // The cell name does not matter as the entire list item is needed.
    forEachCell('.table-row-cell > .date', function(item, entry, listItem) {
      filelist.updateListItemExternalProps(
          listItem,
          this.metadataModel_.getCache(
              [entry], ['availableOffline', 'customIconUrl', 'shared'])[0],
          util.isTeamDriveRoot(entry));
    });
  } else if (type === 'import-history') {
    forEachCell('.table-row-cell > .status', function(item, entry, unused) {
      this.updateStatus_(item, entry);
    });
  }
};

/**
 * Renders table row.
 * @param {function(Entry, cr.ui.Table)} baseRenderFunction Base renderer.
 * @param {Entry} entry Corresponding entry.
 * @return {HTMLLIElement} Created element.
 * @private
 */
FileTable.prototype.renderTableRow_ = function(baseRenderFunction, entry) {
  var item = baseRenderFunction(entry, this);
  var nameId = item.id + '-entry-name';
  var sizeId = item.id + '-size';
  var dateId = item.id + '-date';
  filelist.decorateListItem(item, entry, this.metadataModel_);
  item.setAttribute('file-name', entry.name);
  item.querySelector('.entry-name').setAttribute('id', nameId);
  item.querySelector('.size').setAttribute('id', sizeId);
  item.querySelector('.date').setAttribute('id', dateId);
  item.setAttribute('aria-labelledby', nameId + ' ' + sizeId + ' ' + dateId);
  return item;
};

/**
 * Renders the file thumbnail in the detail table.
 * @param {Entry} entry The Entry object to render.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderThumbnail_ = function(entry) {
  var box = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));
  box.className = 'detail-thumbnail';

  // Set thumbnail if it's already in cache.
  var thumbnailData = this.listThumbnailLoader_ ?
      this.listThumbnailLoader_.getThumbnailFromCache(entry) : null;
  if (thumbnailData && thumbnailData.dataUrl) {
    this.setThumbnailImage_(
        box, this.listThumbnailLoader_.getThumbnailFromCache(entry).dataUrl,
        false /* without animation */);
  }

  return box;
};

/**
 * Sets thumbnail image to the box.
 * @param {!HTMLDivElement} box Detail thumbnail div element.
 * @param {string} dataUrl Data url of thumbnail.
 * @param {boolean} shouldAnimate Whether the thumbnail is shown with animation
 *     or not.
 * @private
 */
FileTable.prototype.setThumbnailImage_ = function(box, dataUrl, shouldAnimate) {
  var oldThumbnails = box.querySelectorAll('.thumbnail');

  var thumbnail = box.ownerDocument.createElement('div');
  thumbnail.classList.add('thumbnail');
  thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';
  thumbnail.addEventListener('animationend', function() {
    // Remove animation css once animation is completed in order not to animate
    // again when an item is attached to the dom again.
    thumbnail.classList.remove('animate');

    for (var i = 0; i < oldThumbnails.length; i++) {
      if (box.contains(oldThumbnails[i]))
        box.removeChild(oldThumbnails[i]);
    }
  });

  if (shouldAnimate)
    thumbnail.classList.add('animate');

  box.appendChild(thumbnail);
};

/**
 * Clears thumbnail image from the box.
 * @param {!HTMLDivElement} box Detail thumbnail div element.
 * @private
 */
FileTable.prototype.clearThumbnailImage_ = function(box) {
  var oldThumbnails = box.querySelectorAll('.thumbnail');

  for (var i = 0; i < oldThumbnails.length; i++) {
    box.removeChild(oldThumbnails[i]);
  }
};

/**
 * Renders the selection checkmark in the detail table.
 * @return {!HTMLDivElement} Created element.
 * @private
 */
FileTable.prototype.renderCheckmark_ = function() {
  var checkmark = /** @type {!HTMLDivElement} */
      (this.ownerDocument.createElement('div'));
  checkmark.className = 'detail-checkmark';
  return checkmark;
};

/**
 * Redraws the UI. Skips multiple consecutive calls.
 */
FileTable.prototype.relayout = function() {
  this.relayoutRateLimiter_.run();
};

/**
 * Redraws the UI immediately.
 * @private
 */
FileTable.prototype.relayoutImmediately_ = function() {
  if (this.clientWidth > 0)
    this.normalizeColumns();
  this.redraw();
  cr.dispatchSimpleEvent(this.list, 'relayout');
};
