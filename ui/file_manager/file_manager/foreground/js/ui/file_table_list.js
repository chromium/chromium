// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';
import {isTeamDriveRoot} from '../../../common/js/entry_utils.js';
import {FileType} from '../../../common/js/file_type.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled, isInlineSyncStatusEnabled} from '../../../common/js/flags.js';
import {getEntryLabel, str, strf} from '../../../common/js/translations.js';
import {EntryLocation} from '../../../externs/entry_location.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {FileListModel} from '../file_list_model.js';
import {MetadataItem} from '../metadata/metadata_item.js';
import {MetadataModel} from '../metadata/metadata_model.js';

import {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import {FileListSelectionModel, FileListSingleSelectionModel} from './file_list_selection_model.js';
import {FileTapHandler} from './file_tap_handler.js';
import {List} from './list.js';
import {ListItem} from './list_item.js';
import {ListSelectionController} from './list_selection_controller.js';
import {ListSelectionModel} from './list_selection_model.js';
import {TableList} from './table/table_list.js';

/**
 * Namespace for utility functions.
 */
const filelist = {};

// Group Heading height, align with CSS #list-container .group-heading.
const GROUP_HEADING_HEIGHT = 57;

/**
 * File table list.
 */
export class FileTableList extends TableList {
  constructor() {
    // To silence closure compiler.
    super();
    /*
     * @type {?function(number, number):void}
     */
    this.onMergeItems_ = null;

    throw new Error('Designed to decorate elements');
  }

  /**
   * Returns the height of group heading.
   * @return {number} The height of group heading.
   */
  getGroupHeadingHeight_() {
    return GROUP_HEADING_HEIGHT;
  }

  /**
   * @param {function(number, number):void} onMergeItems callback called from
// @ts-ignore: error TS7014: Function type, which lacks return-type annotation,
implicitly has an 'any' return type.
   *     |mergeItems| with the parameters |beginIndex| and |endIndex|.
   */
  setOnMergeItems(onMergeItems) {
    assert(!this.onMergeItems_);
    this.onMergeItems_ = onMergeItems;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'endIndex' implicitly has an 'any'
  // type.
  mergeItems(beginIndex, endIndex) {
    // @ts-ignore: error TS2339: Property 'mergeItems' does not exist on type
    // 'TableList'.
    super.mergeItems(beginIndex, endIndex);

    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot =
        fileListModel ? fileListModel.getGroupBySnapshot() : [];
    const startIndexToGroupLabel = new Map(groupBySnapshot.map(group => {
      return [group.startIndex, group];
    }));

    // Make sure that list item's selected attribute is updated just after the
    // mergeItems operation is done. This prevents checkmarks on selected items
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
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        title.innerText = startIndexToGroupLabel.get(i).label;
        title.classList.add(
            'group-heading', `group-by-${fileListModel.groupByField}`);
        this.insertBefore(title, item);
      }
    }

    if (this.onMergeItems_) {
      this.onMergeItems_(beginIndex, endIndex);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'sm' implicitly has an 'any' type.
  createSelectionController(sm) {
    return new FileListSelectionController(assert(sm), this);
  }

  /** @return {A11yAnnounce} */
  get a11y() {
    // @ts-ignore: error TS2339: Property 'a11y' does not exist on type
    // 'Element'.
    return this.table.a11y;
  }

  /**
   * @param {number} index Index of the list item.
   * @return {string}
   */
  getItemLabel(index) {
    // @ts-ignore: error TS2339: Property 'getItemLabel' does not exist on type
    // 'Element'.
    return this.table.getItemLabel(index);
  }

  /**
   * Given a index, return how many group headings are there before this index.
   * Note: not include index itself.
   * @param {number} index
   * @return {number}
   * @private
   */
  getGroupHeadingCountBeforeIndex_(index) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    let count = 0;
    for (const group of groupBySnapshot) {
      // index - 1 because we don't want to include index itself.
      if (group.startIndex <= index - 1) {
        count++;
      } else {
        break;
      }
    }
    return count;
  }

  /**
   * Given a index, return how many group headings are there after this index.
   * Note: not include index itself.
   * @param {number} index
   * @return {number}
   * @private
   */
  getGroupHeadingCountAfterIndex_(index) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    if (groupBySnapshot.length > 0) {
      const countBeforeIndex = this.getGroupHeadingCountBeforeIndex_(index + 1);
      return groupBySnapshot.length - countBeforeIndex;
    }
    return 0;
  }

  /**
   * Given a offset (e.g. scrollTop), return how many items can be included
   * within this height. Override here because previously we just need to use
   * the total height (offset) to divide the item height, now we also need to
   * consider the potential group headings included in these items.
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'offset' implicitly has an 'any' type.
  getIndexForListOffset_(offset) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type 'FileTableList'.
    const itemHeight = this.getDefaultItemHeight_();

    // Without heading the original logic suffices.
    if (groupBySnapshot.length === 0 || !itemHeight) {
      // @ts-ignore: error TS2339: Property 'getIndexForListOffset_' does not
      // exist on type 'TableList'.
      return super.getIndexForListOffset_(offset);
    }

    // Loop through all the groups, calculate the accumulated height for all
    // items (item height + group heading height), until the total height
    // reaches "offset", then we know how many items can be included in this
    // offset.
    let currentHeight = 0;
    for (const group of groupBySnapshot) {
      const groupHeight = this.getGroupHeadingHeight_() +
          (group.endIndex - group.startIndex + 1) * itemHeight;

      if (currentHeight + groupHeight > offset) {
        // Current offset falls into the current group. Calculates how many
        // items in the offset within the group.
        const remainingOffsetInGroup =
            Math.max(0, offset - this.getGroupHeadingHeight_() - currentHeight);
        return group.startIndex +
            Math.floor(remainingOffsetInGroup / itemHeight);
      }
      currentHeight += groupHeight;
    }
    return fileListModel.length - 1;
  }

  /**
   * Given an index, return the height (top) of all items before this index.
   * Override here because previously we just need to use the index to multiply
   * the item height, now we also need to add up the potential group heading
   * heights included in these items.
   *
   * Note: for group start item, technically its height should be "all heights
   * above it + current group heading height", but here we don't add the
   * current group heading height (logic in getGroupHeadingCountBeforeIndex_),
   * that's because it will break the "beforeFillerHeight" logic in the redraw
   * of list.js.
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'index' implicitly has an 'any' type.
  getItemTop(index) {
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type 'FileTableList'.
    const itemHeight = this.getDefaultItemHeight_();
    const countOfGroupHeadings = this.getGroupHeadingCountBeforeIndex_(index);
    return index * itemHeight +
        countOfGroupHeadings * this.getGroupHeadingHeight_();
  }

  /**
   * Given an index, return the height of all items after this index.
   * Override here because previously we just need to use the remaining index
   * to multiply the item height, now we also need to add up the potential
   * group heading heights included in these items.
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'lastIndex' implicitly has an 'any'
  // type.
  getAfterFillerHeight(lastIndex) {
    if (lastIndex === 0) {
      // A special case handled in the parent class, delegate it back to parent.
      return super.getAfterFillerHeight(lastIndex);
    }
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type 'FileTableList'.
    const itemHeight = this.getDefaultItemHeight_();
    const countOfGroupHeadings =
        this.getGroupHeadingCountAfterIndex_(lastIndex);
    const length = this.dataModel?.length ?? 0;
    return (length - lastIndex) * itemHeight +
        countOfGroupHeadings * this.getGroupHeadingHeight_();
  }

  /**
   * Returns whether the drag event is inside a file entry in the list (and not
   * the background padding area).
   * @param {MouseEvent} event Drag start event.
   * @return {boolean} True if the mouse is over an element in the list, False
   *     if it is in the background.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'TableList'.
  hasDragHitElement(event) {
    const pos = DragSelector.getScrolledPosition(this, event);
    // @ts-ignore: error TS2339: Property 'y' does not exist on type 'Object'.
    return this.getHitElements(pos.x, pos.y).length !== 0;
  }

  /**
   * Obtains the index list of elements that are hit by the point or the
   * rectangle.
   *
   * @param {number} x X coordinate value.
   * @param {number} y Y coordinate value.
   * @param {number=} opt_width Width of the coordinate.
   * @param {number=} opt_height Height of the coordinate.
   * @return {!Array<number>} Index list of hit elements.
   */
  // @ts-ignore: error TS6133: 'opt_width' is declared but its value is never
  // read.
  getHitElements(x, y, opt_width, opt_height) {
    const fileListModel = /** @type {FileListModel} */ (this.dataModel);
    const groupBySnapshot =
        fileListModel ? fileListModel.getGroupBySnapshot() : [];
    const startIndexToGroupLabel = new Map(groupBySnapshot.map(group => {
      return [group.startIndex, group];
    }));

    const currentSelection = [];
    const startHeight = y;
    const endHeight = y + (opt_height || 0);
    const length = this.selectionModel?.length ?? 0;
    for (let i = 0; i < length; i++) {
      // @ts-ignore: error TS2339: Property 'getHeightsForIndex' does not exist
      // on type 'FileTableList'.
      const itemMetrics = this.getHeightsForIndex(i);
      // For group start item, we need to explicitly add group height because
      // its top doesn't take that into consideration. (check notes in
      // getItemTop())
      const itemTop = itemMetrics.top +
          (startIndexToGroupLabel.has(i) ? this.getGroupHeadingHeight_() : 0);
      if (itemTop < endHeight && itemTop + itemMetrics.height >= startHeight) {
        currentSelection.push(i);
      }
    }
    return currentSelection;
  }
}

/**
 * Decorates TableList as FileTableList.
 * @param {!TableList} self A table list element.
 */
FileTableList.decorate = self => {
  // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
  // 'TableList'.
  self.__proto__ = FileTableList.prototype;
  // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
  // parameter of type 'string'.
  self.setAttribute('aria-multiselectable', true);
  self.setAttribute('aria-describedby', 'more-actions-info');
  /** @type {FileTableList} */ (self).onMergeItems_ = null;
};

/**
 * Selection controller for the file table list.
 */
// @ts-ignore: error TS2417: Class static side 'typeof
// FileListSelectionController' incorrectly extends base class static side
// 'typeof ListSelectionController'.
class FileListSelectionController extends ListSelectionController {
  /**
   * @param {!ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {!FileTableList} tableList
   */
  constructor(selectionModel, tableList) {
    super(selectionModel);

    /** @const @private @type {!FileTapHandler} */
    this.tapHandler_ = new FileTapHandler();

    /** @const @private @type {!FileTableList} */
    this.tableList_ = tableList;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'index' implicitly has an 'any' type.
  handlePointerDownUp(e, index) {
    filelist.handlePointerDownUp.call(this, e, index);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'index' implicitly has an 'any' type.
  handleTouchEvents(e, index) {
    if (this.tapHandler_.handleTouchEvents(
            assert(e), index, filelist.handleTap.bind(this))) {
      // If a tap event is processed, FileTapHandler cancels the event to
      // prevent triggering click events. Then it results not moving the focus
      // to the list. So we do that here explicitly.
      filelist.focusParentList(e);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleKeyDown(e) {
    filelist.handleKeyDown.call(this, e);
  }

  /** @return {!FileTableList} */
  get filesView() {
    return this.tableList_;
  }
}

/**
 * Common item decoration for table's and grid's items.
 * @param {ListItem} li List item.
 * @param {Entry|FilesAppEntry} entry The entry.
 * @param {!MetadataModel} metadataModel Cache to
 *     retrieve metadata.
 * @param {!VolumeManager} volumeManager Used to retrieve VolumeInfo.
 */
filelist.decorateListItem = (li, entry, metadataModel, volumeManager) => {
  li.classList.add(entry.isDirectory ? 'directory' : 'file');
  // The metadata may not yet be ready. In that case, the list item will be
  // updated when the metadata is ready via updateListItemsMetadata. For files
  // not on an external backend, externalProps is not available.
  // @ts-ignore: error TS2322: Type 'FileSystemEntry | FilesAppEntry' is not
  // assignable to type 'FileSystemEntry'.
  const externalProps = metadataModel.getCache([entry], [
    'hosted',
    'availableOffline',
    'customIconUrl',
    'shared',
    'isMachineRoot',
    'isExternalMedia',
    'pinned',
    'syncStatus',
    'progress',
    'syncCompletedTime',
    'contentMimeType',
    'shortcut',
    'canPin',
    'isDlpRestricted',
    'syncCompletedTime',
  ])[0];
  filelist.updateListItemExternalProps(
      // @ts-ignore: error TS2345: Argument of type 'MetadataItem | undefined'
      // is not assignable to parameter of type 'MetadataItem'.
      li, entry, externalProps, isTeamDriveRoot(entry));

  // Overriding the default role 'list' to 'listbox' for better
  // accessibility on ChromeOS.
  li.setAttribute('role', 'option');
  const disabled = filelist.isDlpBlocked(entry, metadataModel, volumeManager);
  li.toggleAttribute('disabled', disabled);
  if (disabled) {
    li.setAttribute('aria-disabled', 'true');
  } else {
    li.removeAttribute('aria-disabled');
  }

  Object.defineProperty(li, 'selected', {
    /**
     * @this {ListItem}
     * @return {boolean} True if the list item is selected.
     */
    get: function() {
      return this.hasAttribute('selected');
    },

    /**
     * @this {ListItem}
     */
    set: function(v) {
      if (v) {
        this.setAttribute('selected', '');
      } else {
        this.removeAttribute('selected');
      }
    },
  });
};

/**
 * Returns whether `entry` is blocked by DLP.
 *
 * Relies on the fact that volumeManager.isDisabled() can only be true for dirs
 * in file-saveas dialogs, while metadata.isRestrictedForDestination can only be
 * true for files in other types of select dialogs.
 * @param {Entry|FilesAppEntry} entry The entry.
 * @param {!MetadataModel} metadataModel Used to retrieve
 *     isRestrictedForDestination value.
 * @param {!VolumeManager} volumeManager Used to retrieve VolumeInfo and check
 *     if it's disabled.
 * @return {boolean} If `entry` is DLP blocked.
 */
filelist.isDlpBlocked = (entry, metadataModel, volumeManager) => {
  if (!isDlpEnabled()) {
    return false;
  }
  // TODO(b/259184588): Properly handle case when VolumeInfo is not
  // available. E.g. for Crostini we might not have VolumeInfo before it's
  // mounted.
  const volumeInfo = volumeManager.getVolumeInfo(assert(entry));
  if (volumeInfo && volumeManager.isDisabled(volumeInfo.volumeType)) {
    return true;
  }
  const metadata =
      // @ts-ignore: error TS2322: Type 'FileSystemEntry | FilesAppEntry' is not
      // assignable to type 'FileSystemEntry'.
      metadataModel.getCache([entry], ['isRestrictedForDestination'])[0];
  if (metadata && !!metadata.isRestrictedForDestination) {
    return true;
  }
  return false;
};

/**
 * Render the type column of the detail table.
 * @param {!Document} doc Owner document.
 * @param {!Entry} entry The Entry object to render.
 * @param {?EntryLocation} locationInfo
 * @param {string=} opt_mimeType Optional mime type for the file.
 * @return {!HTMLDivElement} Created element.
 */
filelist.renderFileTypeIcon = (doc, entry, locationInfo, opt_mimeType) => {
  const icon = /** @type {!HTMLDivElement} */ (doc.createElement('div'));
  icon.className = 'detail-icon';
  const rootType = locationInfo && locationInfo.rootType || undefined;
  icon.setAttribute(
      'file-type-icon', FileType.getIcon(entry, opt_mimeType, rootType));
  return icon;
};

/**
 * Renders a div beside the row icon that is used to surface badges for
 * individual items in the grid and list view.
 * @param {!Document} doc Owner document.
 * @returns {!HTMLDivElement}
 */
filelist.renderIconBadge = (doc) => {
  const divElement = /** @type {!HTMLDivElement} */ (doc.createElement('div'));
  divElement.classList.add('icon-badge');
  return divElement;
};

/**
 * Render filename label for grid and list view.
 * @param {!Document} doc Owner document.
 * @param {!Entry|!FilesAppEntry} entry The Entry object to render.
 * @param {?EntryLocation} locationInfo
 * @return {!HTMLDivElement} The label.
 */
filelist.renderFileNameLabel = (doc, entry, locationInfo) => {
  // Filename need to be in a '.filename-label' container for correct
  // work of inplace renaming.
  const box = /** @type {!HTMLDivElement} */ (doc.createElement('div'));
  box.className = 'filename-label';
  const fileName = doc.createElement('span');
  fileName.className = 'entry-name';
  fileName.textContent = getEntryLabel(locationInfo, entry);
  box.appendChild(fileName);

  return box;
};

/**
 * Updates grid item or table row for the externalProps.
 * @param {ListItem} li List item.
 * @param {Entry|FilesAppEntry} entry The entry.
 * @param {MetadataItem} externalProps Metadata.
 * @param {boolean} isTeamDriveRoot Whether the entry is a team drive root.
 */
filelist.updateListItemExternalProps =
    (li, entry, externalProps, isTeamDriveRoot) => {
      if (li.classList.contains('file')) {
        li.classList.toggle('dim-hosted', !!externalProps.hosted);
        if (externalProps.contentMimeType) {
          li.classList.toggle(
              'dim-encrypted',
              FileType.isEncrypted(entry, externalProps.contentMimeType));
        }
        const dlpIcon = li.querySelector('.dlp-managed-icon');
        if (dlpIcon) {
          dlpIcon.classList.toggle(
              'is-dlp-restricted', externalProps.isDlpRestricted);
        }
      }

      li.classList.toggle('shortcut', !!externalProps.shortcut);

      const iconDiv = li.querySelector('.detail-icon');
      if (!iconDiv) {
        return;
      }
      // @ts-ignore: error TS2339: Property 'style' does not exist on type
      // 'Element'.
      iconDiv.style.backgroundImage = '';

      if (li.classList.contains('directory')) {
        iconDiv.classList.toggle('shared', !!externalProps.shared);
        iconDiv.classList.toggle('team-drive-root', !!isTeamDriveRoot);
        iconDiv.classList.toggle(
            'computers-root', !!externalProps.isMachineRoot);
        iconDiv.classList.toggle(
            'external-media-root', !!externalProps.isExternalMedia);
      }

      filelist.updateInlineStatus(li, externalProps);
    };

/**
 * Handles tap events on file list to change the selection state.
 *
 * @param {!Event} e The browser mouse event.
 * @param {number} index The index that was under the mouse pointer, -1 if
 *     none.
 * @param {!FileTapHandler.TapEvent} eventType
 * @return True if conducted any action. False when if did nothing special for
 *     tap.
 * @this {ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handleTap = function(e, index, eventType) {
  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);

  if (eventType === FileTapHandler.TapEvent.TWO_FINGER_TAP) {
    // Prepare to open the context menu in the same manner as the right click.
    // If the target is any of the selected files, open a one for those files.
    // If the target is a non-selected file, cancel current selection and open
    // context menu for the single file.
    // Otherwise (when the target is the background), for the current folder.
    if (index === -1) {
      // Two-finger tap outside the list should be handled here because it does
      // not produce mousedown/click events.
      // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
      // 'ListSelectionController'.
      this.filesView.a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
      sm.unselectAll();
    } else {
      const indexSelected = sm.getIndexSelected(index);
      if (!indexSelected) {
        // Prepare to open context menu of the new item by selecting only it.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.unselectAll();
        }
        sm.beginChange();
        sm.selectedIndex = index;
        sm.endChange();
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        const name = this.filesView.getItemLabel(index);
      }
    }

    // Context menu will be opened for the selected files by the following
    // 'contextmenu' event.
    return false;
  }

  if (index === -1) {
    return false;
  }

  // Single finger tap.
  const isTap = eventType === FileTapHandler.TapEvent.TAP ||
      eventType === FileTapHandler.TapEvent.LONG_TAP;
  // Revert to click handling for single tap on the checkmark or rename input.
  // Single tap on the item checkmark should toggle select the item.
  // Single tap on rename input should focus on input.
  // @ts-ignore: error TS2339: Property 'classList' does not exist on type
  // 'EventTarget'.
  const isCheckmark = e.target.classList.contains('detail-checkmark') ||
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'EventTarget'.
      e.target.classList.contains('detail-icon');
  // @ts-ignore: error TS2339: Property 'localName' does not exist on type
  // 'EventTarget'.
  const isRename = e.target.localName === 'input';
  if (eventType === FileTapHandler.TapEvent.TAP && (isCheckmark || isRename)) {
    return false;
  }

  // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
  // 'Event'.
  if (sm.multiple && sm.getCheckSelectMode() && isTap && !e.shiftKey) {
    // toggle item selection. Equivalent to mouse click on checkbox.
    sm.beginChange();

    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    const name = this.filesView.getItemLabel(index);
    const msgId = sm.getIndexSelected(index) ? 'SELECTION_ADD_SINGLE_ENTRY' :
                                               'SELECTION_REMOVE_SINGLE_ENTRY';
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    this.filesView.a11y.speakA11yMessage(strf(msgId, name));

    sm.setIndexSelected(index, !sm.getIndexSelected(index));
    // Toggle the current one and make it anchor index.
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
  } else if (
      sm.multiple && (eventType === FileTapHandler.TapEvent.LONG_PRESS)) {
    sm.beginChange();
    if (!sm.getCheckSelectMode()) {
      // Make sure to unselect the leading item that was not the touch target.
      sm.unselectAll();
      sm.setCheckSelectMode(true);
    }
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    const name = this.filesView.getItemLabel(index);
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
    // Do not toggle selection yet, so as to avoid unselecting before drag.
  } else if (
      eventType === FileTapHandler.TapEvent.TAP && !sm.getCheckSelectMode()) {
    // Single tap should open the item with default action.
    // Select the item, so that MainWindowComponent will execute action of it.
    sm.beginChange();
    sm.unselectAll();
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    const name = this.filesView.getItemLabel(index);
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
  }
  return false;
};

/**
 * Handles mouseup/mousedown events on file list to change the selection state.
 *
 * Basically the content of this function is identical to
 * ListSelectionController's handlePointerDownUp(), but following
 * handlings are inserted to control the check-select mode.
 *
 * 1) When checkmark area is clicked, toggle item selection and enable the
 *    check-select mode.
 * 2) When non-checkmark area is clicked in check-select mode, disable the
 *    check-select mode.
 *
 * @param {!Event} e The browser mouse event.
 * @param {number} index The index that was under the mouse pointer, -1 if
 *     none.
 * @this {ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handlePointerDownUp = function(e, index) {
  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);

  const anchorIndex = sm.anchorIndex;
  const isDown = (e.type === 'mousedown');

  // @ts-ignore: error TS2339: Property 'classList' does not exist on type
  // 'EventTarget'.
  const isTargetCheckmark = e.target.classList.contains('detail-checkmark') ||
      // @ts-ignore: error TS2339: Property 'classList' does not exist on type
      // 'EventTarget'.
      e.target.classList.contains('checkmark');
  // If multiple selection is allowed and the checkmark is clicked without
  // modifiers(Ctrl/Shift), the click should toggle the item's selection.
  // (i.e. same behavior as Ctrl+Click)
  const isClickOnCheckmark =
      // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
      // 'Event'.
      (isTargetCheckmark && sm.multiple && index !== -1 && !e.shiftKey &&
       // @ts-ignore: error TS2339: Property 'button' does not exist on type
       // 'Event'.
       !e.ctrlKey && e.button === 0);

  sm.beginChange();

  if (index === -1) {
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    this.filesView.a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.leadIndex = sm.anchorIndex = -1;
    sm.unselectAll();
  } else {
    // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
    // 'Event'.
    if (sm.multiple && (e.ctrlKey || isClickOnCheckmark) && !e.shiftKey) {
      // Selection is handled at mouseUp.
      if (!isDown) {
        // 1) When checkmark area is clicked, toggle item selection and enable
        //    the check-select mode.
        if (isClickOnCheckmark) {
          // If Files app enters check-select mode by clicking an item's icon,
          // existing selection should be cleared.
          if (!sm.getCheckSelectMode()) {
            sm.unselectAll();
          }
        }
        // Always enables check-select mode when the selection is updated by
        // Ctrl+Click or Click on an item's icon.
        sm.setCheckSelectMode(true);

        // Toggle the current one and make it anchor index.
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        const name = this.filesView.getItemLabel(index);
        const msgId = sm.getIndexSelected(index) ?
            'SELECTION_REMOVE_SINGLE_ENTRY' :
            'SELECTION_ADD_SINGLE_ENTRY';
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        this.filesView.a11y.speakA11yMessage(strf(msgId, name));
        sm.setIndexSelected(index, !sm.getIndexSelected(index));
        sm.leadIndex = index;
        sm.anchorIndex = index;
      }
      // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
      // 'Event'.
    } else if (e.shiftKey && anchorIndex !== -1 && anchorIndex !== index) {
      // Shift is done in mousedown.
      if (isDown) {
        sm.unselectAll();
        sm.leadIndex = index;
        if (sm.multiple) {
          sm.selectRange(anchorIndex, index);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const nameStart = this.filesView.getItemLabel(anchorIndex);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const nameEnd = this.filesView.getItemLabel(index);
          const count = Math.abs(index - anchorIndex) + 1;
          const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          this.filesView.a11y.speakA11yMessage(msg);
        } else {
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const name = this.filesView.getItemLabel(index);
          sm.setIndexSelected(index, true);
        }
      }
    } else {
      // Right click for a context menu needs to not clear the selection.
      // @ts-ignore: error TS2339: Property 'button' does not exist on type
      // 'Event'.
      const isRightClick = e.button === 2;

      // If the index is selected this is handled in mouseup.
      const indexSelected = sm.getIndexSelected(index);
      if ((indexSelected && !isDown || !indexSelected && isDown) &&
          !(indexSelected && isRightClick)) {
        // 2) When non-checkmark area is clicked in check-select mode, disable
        //    the check-select mode.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.endChange();
          sm.unselectAll();
          sm.beginChange();
        }
        // This event handler is called for mouseup and mousedown, let's
        // announce the selection only in one of them.
        if (isDown) {
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const name = this.filesView.getItemLabel(index);
        }
        sm.selectedIndex = index;
      }
    }
  }
  sm.endChange();
};

/**
 * Handles key events on file list to change the selection state.
 *
 * Basically the content of this function is identical to
 * ListSelectionController's handleKeyDown(), but following handlings is
 * inserted to control the check-select mode.
 *
 * 1) When pressing direction key results in a single selection, the
 *    check-select mode should be terminated.
 *
 * @param {Event} e The keydown event.
 * @this {ListSelectionController} either FileListSelectionController or
 *     FileGridSelectionController.
 */
filelist.handleKeyDown = function(e) {
  // @ts-ignore: error TS2339: Property 'tagName' does not exist on type
  // 'EventTarget'.
  const tagName = e.target.tagName;

  // If focus is in an input field of some kind, only handle navigation keys
  // that aren't likely to conflict with input interaction (e.g., text
  // editing, or changing the value of a checkbox or select).
  if (tagName === 'INPUT') {
    // @ts-ignore: error TS2339: Property 'type' does not exist on type
    // 'EventTarget'.
    const inputType = e.target.type;
    // Just protect space (for toggling) for checkbox and radio.
    if (inputType === 'checkbox' || inputType === 'radio') {
      // @ts-ignore: error TS2339: Property 'key' does not exist on type
      // 'Event'.
      if (e.key === ' ') {
        return;
      }
      // Protect all but the most basic navigation commands in anything else.
      // @ts-ignore: error TS2339: Property 'key' does not exist on type
      // 'Event'.
    } else if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') {
      return;
    }
  }
  // Similarly, don't interfere with select element handling.
  if (tagName === 'SELECT') {
    return;
  }

  const sm = /**
                @type {!FileListSelectionModel|!FileListSingleSelectionModel}
                  */
      (this.selectionModel);
  let newIndex = -1;
  const leadIndex = sm.leadIndex;
  let prevent = true;

  // Ctrl/Meta+A. Use keyCode=65 to use the same shortcut key regardless of
  // keyboard layout.
  // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
  const pressedKeyA = e.keyCode === 65 || e.key === 'a';
  // @ts-ignore: error TS2339: Property 'ctrlKey' does not exist on type
  // 'Event'.
  if (sm.multiple && pressedKeyA && e.ctrlKey) {
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    this.filesView.a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
    sm.setCheckSelectMode(true);
    sm.selectAll();
    e.preventDefault();
    return;
  }

  // Esc
  // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
  // 'Event'.
  if (e.key === 'Escape' && !e.ctrlKey && !e.shiftKey) {
    // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
    // 'ListSelectionController'.
    this.filesView.a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.unselectAll();
    e.preventDefault();
    return;
  }

  // Space: Note ChromeOS and ChromeOS on Linux can generate KeyDown Space
  // events differently the |key| attribute might be set to 'Unidentified'.
  // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
  if (e.code === 'Space' || e.key === ' ') {
    if (leadIndex !== -1) {
      const selected = sm.getIndexSelected(leadIndex);
      // @ts-ignore: error TS2339: Property 'ctrlKey' does not exist on type
      // 'Event'.
      if (e.ctrlKey) {
        sm.beginChange();

        // Force selecting if it's the first item selected, otherwise flip the
        // "selected" status.
        if (selected && sm.selectedIndexes.length === 1 &&
            !sm.getCheckSelectMode()) {
          // It needs to go back/forth to trigger the 'change' event.
          sm.setIndexSelected(leadIndex, false);
          sm.setIndexSelected(leadIndex, true);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const name = this.filesView.getItemLabel(leadIndex);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          this.filesView.a11y.speakA11yMessage(
              strf('SELECTION_SINGLE_ENTRY', name));
        } else {
          // Toggle the current one and make it anchor index.
          sm.setIndexSelected(leadIndex, !selected);
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          const name = this.filesView.getItemLabel(leadIndex);
          const msgId = selected ? 'SELECTION_REMOVE_SINGLE_ENTRY' :
                                   'SELECTION_ADD_SINGLE_ENTRY';
          // @ts-ignore: error TS2339: Property 'filesView' does not exist on
          // type 'ListSelectionController'.
          this.filesView.a11y.speakA11yMessage(strf(msgId, name));
        }

        // Force check-select, FileListSelectionModel.onChangeEvent_ resets it
        // if needed.
        sm.setCheckSelectMode(true);
        sm.endChange();

        // Prevents space to opening quickview.
        e.stopPropagation();
        e.preventDefault();
        return;
      }
    }
  }

  // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
  switch (e.key) {
    case 'Home':
      newIndex = this.getFirstIndex();
      break;
    case 'End':
      newIndex = this.getLastIndex();
      break;
    case 'ArrowUp':
      newIndex = leadIndex === -1 ? this.getLastIndex() :
                                    this.getIndexAbove(leadIndex);
      break;
    case 'ArrowDown':
      newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                    this.getIndexBelow(leadIndex);
      break;
    case 'ArrowLeft':
    case 'MediaTrackPrevious':
      newIndex = leadIndex === -1 ? this.getLastIndex() :
                                    this.getIndexBefore(leadIndex);
      break;
    case 'ArrowRight':
    case 'MediaTrackNext':
      newIndex = leadIndex === -1 ? this.getFirstIndex() :
                                    this.getIndexAfter(leadIndex);
      break;
    default:
      prevent = false;
  }

  if (newIndex >= 0 && newIndex < sm.length) {
    sm.beginChange();

    sm.leadIndex = newIndex;
    // @ts-ignore: error TS2339: Property 'shiftKey' does not exist on type
    // 'Event'.
    if (e.shiftKey) {
      const anchorIndex = sm.anchorIndex;
      if (sm.multiple) {
        sm.unselectAll();
      }
      if (anchorIndex === -1) {
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        const name = this.filesView.getItemLabel(newIndex);
        sm.setIndexSelected(newIndex, true);
        sm.anchorIndex = newIndex;
      } else {
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        const nameStart = this.filesView.getItemLabel(anchorIndex);
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        const nameEnd = this.filesView.getItemLabel(newIndex);
        const count = Math.abs(newIndex - anchorIndex) + 1;
        const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
        // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
        // 'ListSelectionController'.
        this.filesView.a11y.speakA11yMessage(msg);
        sm.selectRange(anchorIndex, newIndex);
      }
      // @ts-ignore: error TS2339: Property 'ctrlKey' does not exist on type
      // 'Event'.
    } else if (e.ctrlKey) {
      // While Ctrl is being held, only leadIndex and anchorIndex are moved.
      sm.anchorIndex = newIndex;
    } else {
      // 1) When pressing direction key results in a single selection, the
      //    check-select mode should be terminated.
      sm.setCheckSelectMode(false);

      if (sm.multiple) {
        sm.unselectAll();
      }
      // @ts-ignore: error TS2339: Property 'filesView' does not exist on type
      // 'ListSelectionController'.
      const name = this.filesView.getItemLabel(newIndex);
      sm.setIndexSelected(newIndex, true);
      sm.anchorIndex = newIndex;
    }

    sm.endChange();

    if (prevent) {
      e.preventDefault();
    }
  }
};

/**
 * Focus on the file list that contains the event target.
 * @param {!Event} event the touch event.
 */
filelist.focusParentList = event => {
  let element = event.target;
  while (element && !(element instanceof List)) {
    // @ts-ignore: error TS2339: Property 'parentElement' does not exist on type
    // 'EventTarget'.
    element = element.parentElement;
  }
  if (element) {
    element.focus();
  }
};

/**
 * Update the item's inline status when it's restored from List's cache..
 * @param {!ListItem} restoredItem Item being restored from the List cache.
 * @param {ArrayDataModel} dataModel Data model corresponding to the item.
 * @param {MetadataModel} metadataModel Cache to retrieve metadata.
 */
filelist.updateCacheItemInlineStatus =
    (restoredItem, dataModel, metadataModel) => {
      if (!dataModel || !metadataModel) {
        console.error('dataModel or metadataModel unavailable.');
        return;
      }

      const entry = dataModel.item(restoredItem.listIndex);

      const metadata = metadataModel.getCache([entry], [
        'availableOffline',
        'pinned',
        'canPin',
        'syncStatus',
        'progress',
        'syncCompletedTime',
      ])[0];

      // @ts-ignore: error TS2345: Argument of type 'MetadataItem | undefined'
      // is not assignable to parameter of type 'MetadataItem | null'.
      filelist.updateInlineStatus(restoredItem, metadata);
    };

/**
 * Update status icon for file or directory entry.
 * @param {!HTMLLIElement} li The grid item.
 * @param {?MetadataItem} metadata Metadata.
 */
filelist.updateInlineStatus = (li, metadata) => {
  const inlineStatus = li.querySelector('xf-inline-status');

  if (!metadata || !inlineStatus) {
    return;
  }

  const {
    pinned,
    availableOffline,
    // @ts-ignore: error TS2339: Property 'canPin' does not exist on type
    // 'MetadataItem'.
    canPin,
    progress,
    syncStatus,
    syncCompletedTime,
  } = metadata;

  if (isDriveFsBulkPinningEnabled()) {
    const cantPin = canPin === false;
    li.classList.toggle('cant-pin', cantPin);
    inlineStatus.toggleAttribute('cant-pin', cantPin);
  }

  // Directories are always displayed as available offline.
  const dimOffline =
      li.classList.contains('file') && availableOffline === false;
  li.classList.toggle('dim-offline', dimOffline);
  li.classList.toggle('pinned', pinned);
  inlineStatus.toggleAttribute('available-offline', pinned && !dimOffline);

  if (isInlineSyncStatusEnabled()) {
    let actualSyncStatus = syncStatus;
    let actualProgress = progress;
    // Force sync status as completed if it has been less than 300ms since the
    // file has completed syncing.
    // @ts-ignore: error TS18048: 'syncCompletedTime' is possibly 'undefined'.
    if (Date.now() - syncCompletedTime < 300) {
      actualSyncStatus = chrome.fileManagerPrivate.SyncStatus.COMPLETED;
      actualProgress = 1;
    }
    inlineStatus.setAttribute('sync-status', String(actualSyncStatus));
    inlineStatus.setAttribute('progress', String(actualProgress));
  }
};

export {filelist};
