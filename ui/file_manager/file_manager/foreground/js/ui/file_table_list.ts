// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {EntryLocation} from '../../../background/js/entry_location_impl.js';
import type {VolumeManager} from '../../../background/js/volume_manager.js';
import type {ArrayDataModel} from '../../../common/js/array_data_model.js';
import {isTeamDriveRoot} from '../../../common/js/entry_utils.js';
import {getIcon, isEncrypted} from '../../../common/js/file_type.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled} from '../../../common/js/flags.js';
import {getEntryLabel, str, strf} from '../../../common/js/translations.js';
import type {FileListModel} from '../file_list_model.js';
import type {MetadataItem} from '../metadata/metadata_item.js';
import type {MetadataModel} from '../metadata/metadata_model.js';

import type {A11yAnnounce} from './a11y_announce.js';
import {DragSelector} from './drag_selector.js';
import type {FileGridSelectionController} from './file_grid.js';
import type {FileListSelectionModel} from './file_list_selection_model.js';
import type {FileTable} from './file_table.js';
import {FileTapHandler, TapEvent} from './file_tap_handler.js';
import {List} from './list.js';
import type {ListItem} from './list_item.js';
import {ListSelectionController} from './list_selection_controller.js';
import type {ListSelectionModel} from './list_selection_model.js';
import {TableList} from './table/table_list.js';

// Group Heading height, align with CSS #list-container .group-heading.
const GROUP_HEADING_HEIGHT = 57;

type OnMergeItemsCallback = (beginIndex: number, endIndex: number) => void;

/**
 * File table list.
 */
export class FileTableList extends TableList {
  private onMergeItems_: null|OnMergeItemsCallback = null;
  shouldStartDragSelection: null|((e: MouseEvent) => boolean) = null;

  override initialize() {
    this.setAttribute('aria-multiselectable', 'true');
    this.setAttribute('aria-describedby', 'more-actions-info');
    this.onMergeItems_ = null;
  }

  override get table(): FileTable {
    return super.table as FileTable;
  }

  override get dataModel(): FileListModel {
    return super.dataModel as FileListModel;
  }

  override set dataModel(value: FileListModel) {
    super.dataModel = value;
  }

  /**
   * Returns the height of group heading.
   */
  private getGroupHeadingHeight_(): number {
    return GROUP_HEADING_HEIGHT;
  }

  /**
   * @param onMergeItems callback called from `mergeItems` with the
   *     parameters `beginIndex` and `endIndex`.
   */
  setOnMergeItems(onMergeItems: OnMergeItemsCallback) {
    assert(!this.onMergeItems_);
    this.onMergeItems_ = onMergeItems;
  }

  override mergeItems(beginIndex: number, endIndex: number) {
    super.mergeItems(beginIndex, endIndex);

    const fileListModel = this.dataModel;
    const groupBySnapshot =
        fileListModel ? fileListModel.getGroupBySnapshot() : [];
    const startIndexToGroupLabel = new Map(groupBySnapshot.map(group => {
      return [group.startIndex, group];
    }));

    // Make sure that list item's selected attribute is updated just after
    // the mergeItems operation is done. This prevents checkmarks on
    // selected items from being animated unintentionally by redraw.
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
        // For first item in each group, we add a title div before the
        // element.
        const title = document.createElement('div');
        title.setAttribute('role', 'heading');
        title.innerText = startIndexToGroupLabel.get(i)!.label;
        title.classList.add(
            'group-heading', `group-by-${fileListModel.groupByField}`);
        this.insertBefore(title, item);
      }
    }

    if (this.onMergeItems_) {
      this.onMergeItems_(beginIndex, endIndex);
    }
  }

  override createSelectionController(sm: ListSelectionModel):
      FileListSelectionController {
    assert(sm);
    return new FileListSelectionController(sm, this);
  }

  get a11y(): A11yAnnounce {
    return this.table.a11y!;
  }

  /**
   * @param index Index of the list item.
   */
  getItemLabel(index: number): string {
    return this.table.getItemLabel(index);
  }

  /**
   * Given a index, return how many group headings are there before this
   * index. Note: not include index itself.
   */
  private getGroupHeadingCountBeforeIndex_(index: number): number {
    const groupBySnapshot = this.dataModel.getGroupBySnapshot();
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
   * Given a index, return how many group headings are there after this
   * index. Note: not include index itself.
   */
  private getGroupHeadingCountAfterIndex_(index: number): number {
    const groupBySnapshot = this.dataModel.getGroupBySnapshot();
    if (groupBySnapshot.length > 0) {
      const countBeforeIndex = this.getGroupHeadingCountBeforeIndex_(index + 1);
      return groupBySnapshot.length - countBeforeIndex;
    }
    return 0;
  }

  /**
   * Given a offset (e.g. scrollTop), return how many items can be included
   * within this height. Override here because previously we just need to
   * use the total height (offset) to divide the item height, now we also
   * need to consider the potential group headings included in these items.
   */
  protected override getIndexForListOffset_(offset: number) {
    const fileListModel = this.dataModel;
    const groupBySnapshot = fileListModel.getGroupBySnapshot();
    const itemHeight = this.getDefaultItemHeight_();

    // Without heading the original logic suffices.
    if (groupBySnapshot.length === 0 || !itemHeight) {
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
   * Override here because previously we just need to use the index to
   * multiply the item height, now we also need to add up the potential
   * group heading heights included in these items.
   *
   * Note: for group start item, technically its height should be "all
   * heights above it + current group heading height", but here we don't add
   * the current group heading height (logic in
   * getGroupHeadingCountBeforeIndex_), that's because it will break the
   * "beforeFillerHeight" logic in the redraw of list.js.
   */
  override getItemTop(index: number) {
    const itemHeight = this.getDefaultItemHeight_();
    const countOfGroupHeadings = this.getGroupHeadingCountBeforeIndex_(index);
    return index * itemHeight +
        countOfGroupHeadings * this.getGroupHeadingHeight_();
  }

  /**
   * Given an index, return the height of all items after this index.
   * Override here because previously we just need to use the remaining
   * index to multiply the item height, now we also need to add up the
   * potential group heading heights included in these items.
   */
  override getAfterFillerHeight(lastIndex: number) {
    if (lastIndex === 0) {
      // A special case handled in the parent class, delegate it back to
      // parent.
      return super.getAfterFillerHeight(lastIndex);
    }
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
   * @param event Drag start event.
   * @return True if the mouse is over an element in the list, False if it is in
   *     the background.
   */
  hasDragHitElement(event: MouseEvent): boolean {
    const pos = DragSelector.getScrolledPosition(this, event)!;
    return this.getHitElements(pos.x, pos.y).length !== 0;
  }

  /**
   * Obtains the index list of elements that are hit by the point or the
   * rectangle.
   *
   * @param _x X coordinate value.
   * @param y Y coordinate value.
   * @param _width Width of the coordinate.
   * @param height Height of the coordinate.
   * @return Index list of hit elements.
   */
  override getHitElements(
      _x: number, y: number, _width?: number, height?: number): number[] {
    const fileListModel = this.dataModel;
    const groupBySnapshot =
        fileListModel ? fileListModel.getGroupBySnapshot() : [];
    const startIndexToGroupLabel = new Map(groupBySnapshot.map(group => {
      return [group.startIndex, group];
    }));

    const currentSelection = [];
    const startHeight = y;
    const endHeight = y + (height || 0);
    const length = this.selectionModel?.length ?? 0;
    for (let i = 0; i < length; i++) {
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
 * Selection controller for the file table list.
 */
class FileListSelectionController extends ListSelectionController {
  private readonly tapHandler_ = new FileTapHandler();


  /**
   * @param selectionModel The selection model to
   *     interact with.
   */
  constructor(
      selectionModel: ListSelectionModel, private tableList_: FileTableList) {
    super(selectionModel);
  }

  override handlePointerDownUp(e: PointerEvent, index: number) {
    handlePointerDownUp.call(this, e, index);
  }

  override handleTouchEvents(e: TouchEvent, index: number) {
    if (this.tapHandler_.handleTouchEvents(e, index, handleTap.bind(this))) {
      // If a tap event is processed, FileTapHandler cancels the event to
      // prevent triggering click events. Then it results not moving the focus
      // to the list. So we do that here explicitly.
      focusParentList(e);
    }
  }

  override handleKeyDown(e: KeyboardEvent) {
    handleKeyDown.call(this, e);
  }

  get filesView(): FileTableList {
    return this.tableList_;
  }
}

/**
 * Common item decoration for table's and grid's items.
 * @param li List item.
 * @param entry The entry.
 * @param metadataModel Cache to
 *     retrieve metadata.
 * @param volumeManager Used to retrieve VolumeInfo.
 */
export function decorateListItem(
    li: ListItem, entry: Entry|FilesAppEntry, metadataModel: MetadataModel,
    volumeManager: VolumeManager) {
  li.classList.add(entry.isDirectory ? 'directory' : 'file');
  // The metadata may not yet be ready. In that case, the list item will be
  // updated when the metadata is ready via updateListItemsMetadata. For
  // files not on an external backend, externalProps is not available.
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
  ])[0]!;
  updateListItemExternalProps(li, entry, externalProps, isTeamDriveRoot(entry));

  // Overriding the default role 'list' to 'listbox' for better
  // accessibility on ChromeOS.
  li.setAttribute('role', 'option');
  const disabled = isDlpBlocked(entry, metadataModel, volumeManager);
  li.toggleAttribute('disabled', disabled);
  if (disabled) {
    li.setAttribute('aria-disabled', 'true');
  } else {
    li.removeAttribute('aria-disabled');
  }
}

/**
 * Returns whether `entry` is blocked by DLP.
 *
 * Relies on the fact that volumeManager.isDisabled() can only be true for
 * dirs in file-saveas dialogs, while metadata.isRestrictedForDestination
 * can only be true for files in other types of select dialogs.
 * @param entry The entry.
 * @param metadataModel Used to retrieve isRestrictedForDestination value.
 * @param volumeManager Used to retrieve VolumeInfo and check if it's disabled.
 */
export function isDlpBlocked(
    entry: Entry|FilesAppEntry, metadataModel: MetadataModel,
    volumeManager: VolumeManager): boolean {
  if (!isDlpEnabled()) {
    return false;
  }
  // TODO(b/259184588): Properly handle case when VolumeInfo is not
  // available. E.g. for Crostini we might not have VolumeInfo before it's
  // mounted.
  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (volumeInfo && volumeManager.isDisabled(volumeInfo.volumeType)) {
    return true;
  }
  const metadata =
      metadataModel.getCache([entry], ['isRestrictedForDestination'])[0];
  if (metadata && !!metadata.isRestrictedForDestination) {
    return true;
  }
  return false;
}

/**
 * Render the type column of the detail table.
 * @param doc Owner document.
 * @param entry The Entry object to render.
 * @param mimeType Optional mime type for the file.
 * @return Created element.
 */
export function renderFileTypeIcon(
    doc: Document, entry: Entry, locationInfo: null|EntryLocation,
    mimeType?: string): HTMLDivElement {
  const icon = doc.createElement('div');
  icon.className = 'detail-icon';
  const rootType = locationInfo?.rootType;
  icon.setAttribute('file-type-icon', getIcon(entry, mimeType, rootType));
  return icon;
}

/**
 * Renders a div beside the row icon that is used to surface badges for
 * individual items in the grid and list view.
 * @param doc Owner document.
 */
export function renderIconBadge(doc: Document): HTMLDivElement {
  const divElement = doc.createElement('div');
  divElement.classList.add('icon-badge');
  return divElement;
}

/**
 * Render filename label for grid and list view.
 * @param doc Owner document.
 * @param entry The Entry object to render.
 * @return The label element.
 */
export function renderFileNameLabel(
    doc: Document, entry: Entry|FilesAppEntry,
    locationInfo: null|EntryLocation): HTMLDivElement {
  // Filename need to be in a '.filename-label' container for correct
  // work of inplace renaming.
  const box = doc.createElement('div');
  box.className = 'filename-label';
  const fileName = doc.createElement('span');
  fileName.className = 'entry-name';
  fileName.textContent = getEntryLabel(locationInfo, entry);
  box.appendChild(fileName);

  return box;
}

/**
 * Updates grid item or table row for the externalProps.
 * @param li List item.
 * @param entry The entry.
 * @param externalProps Metadata.
 * @param isTeamDriveRoot Whether the entry is a team drive root.
 */
export function updateListItemExternalProps(
    li: ListItem, entry: Entry|FilesAppEntry, externalProps: MetadataItem,
    isTeamDriveRoot: boolean) {
  if (li.classList.contains('file')) {
    li.classList.toggle('dim-hosted', !!externalProps.hosted);
    if (externalProps.contentMimeType) {
      li.classList.toggle(
          'dim-encrypted', isEncrypted(entry, externalProps.contentMimeType));
    }
    const dlpIcon = li.querySelector('.dlp-managed-icon');
    if (dlpIcon) {
      dlpIcon.classList.toggle(
          'is-dlp-restricted', externalProps.isDlpRestricted);
    }
  }

  li.classList.toggle('shortcut', !!externalProps.shortcut);

  const iconDiv = li.querySelector<HTMLElement>('.detail-icon');
  if (!iconDiv) {
    return;
  }
  iconDiv.style.backgroundImage = '';

  if (li.classList.contains('directory')) {
    iconDiv.classList.toggle('shared', !!externalProps.shared);
    iconDiv.classList.toggle('team-drive-root', !!isTeamDriveRoot);
    iconDiv.classList.toggle('computers-root', !!externalProps.isMachineRoot);
    iconDiv.classList.toggle(
        'external-media-root', !!externalProps.isExternalMedia);
  }

  updateInlineStatus(li, externalProps);
}

/**
 * Handles tap events on file list to change the selection state.
 *
 * @param e The browser mouse event.
 * @param index The index that was under the mouse pointer, -1 if none.
 * @return True if conducted any action. False when if did nothing special for
 *     tap.
 */
export function handleTap(
    this: FileListSelectionController|FileGridSelectionController,
    e: TouchEvent, index: number, eventType: TapEvent) {
  const sm = this.selectionModel as FileListSelectionModel;
  const a11y = this.filesView.a11y!;

  if (eventType === TapEvent.TWO_FINGER_TAP) {
    // Prepare to open the context menu in the same manner as the right
    // click. If the target is any of the selected files, open a one for
    // those files. If the target is a non-selected file, cancel current
    // selection and open context menu for the single file. Otherwise (when
    // the target is the background), for the current folder.
    if (index === -1) {
      // Two-finger tap outside the list should be handled here because it
      // does not produce mousedown/click events.
      a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
      sm.unselectAll();
    } else {
      const indexSelected = sm.getIndexSelected(index);
      if (!indexSelected) {
        // Prepare to open context menu of the new item by selecting only
        // it.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.unselectAll();
        }
        sm.beginChange();
        sm.selectedIndex = index;
        sm.endChange();
      }
    }

    // Context menu will be opened for the selected files by the following
    // 'contextmenu' event.
    return false;
  }

  if (index === -1) {
    return false;
  }

  const target = e.target as HTMLElement;
  // Single finger tap.
  const isTap = eventType === TapEvent.TAP || eventType === TapEvent.LONG_TAP;
  // Revert to click handling for single tap on the checkmark or rename
  // input. Single tap on the item checkmark should toggle select the item.
  // Single tap on rename input should focus on input.
  const isCheckmark = target.classList.contains('detail-checkmark') ||
      target.classList.contains('detail-icon');
  const isRename = target.localName === 'input';
  if (eventType === TapEvent.TAP && (isCheckmark || isRename)) {
    return false;
  }

  if (sm.multiple && sm.getCheckSelectMode() && isTap && !e.shiftKey) {
    // toggle item selection. Equivalent to mouse click on checkbox.
    sm.beginChange();

    const name = this.filesView.getItemLabel(index);
    const msgId = sm.getIndexSelected(index) ? 'SELECTION_ADD_SINGLE_ENTRY' :
                                               'SELECTION_REMOVE_SINGLE_ENTRY';
    a11y.speakA11yMessage(strf(msgId, name));

    sm.setIndexSelected(index, !sm.getIndexSelected(index));
    // Toggle the current one and make it anchor index.
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
  } else if (sm.multiple && (eventType === TapEvent.LONG_PRESS)) {
    sm.beginChange();
    if (!sm.getCheckSelectMode()) {
      // Make sure to unselect the leading item that was not the touch
      // target.
      sm.unselectAll();
      sm.setCheckSelectMode(true);
    }
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
    return true;
    // Do not toggle selection yet, so as to avoid unselecting before drag.
  } else if (eventType === TapEvent.TAP && !sm.getCheckSelectMode()) {
    // Single tap should open the item with default action.
    // Select the item, so that MainWindowComponent will execute action of
    // it.
    sm.beginChange();
    sm.unselectAll();
    sm.setIndexSelected(index, true);
    sm.leadIndex = index;
    sm.anchorIndex = index;
    sm.endChange();
  }
  return false;
}

/**
 * Handles mouseup/mousedown events on file list to change the selection
 * state.
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
 * @param e The browser mouse event.
 * @param index The index that was under the mouse pointer, -1 if
 *     none.
 */
export function handlePointerDownUp(
    this: FileListSelectionController|FileGridSelectionController,
    e: MouseEvent, index: number) {
  const sm = this.selectionModel as FileListSelectionModel;
  const anchorIndex = sm.anchorIndex;
  const isDown = (e.type === 'mousedown');

  const target = e.target as HTMLElement;
  const isTargetCheckmark = target.classList.contains('detail-checkmark') ||
      target.classList.contains('checkmark');
  // If multiple selection is allowed and the checkmark is clicked without
  // modifiers(Ctrl/Shift), the click should toggle the item's selection.
  // (i.e. same behavior as Ctrl+Click)
  const isClickOnCheckmark =
      (isTargetCheckmark && sm.multiple && index !== -1 && !e.shiftKey &&
       !e.ctrlKey && e.button === 0);

  sm.beginChange();

  const a11y = this.filesView.a11y!;
  if (index === -1) {
    a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.leadIndex = sm.anchorIndex = -1;
    sm.unselectAll();
  } else {
    if (sm.multiple && (e.ctrlKey || isClickOnCheckmark) && !e.shiftKey) {
      // Selection is handled at mouseUp.
      if (!isDown) {
        // 1) When checkmark area is clicked, toggle item selection and
        // enable
        //    the check-select mode.
        if (isClickOnCheckmark) {
          // If Files app enters check-select mode by clicking an item's
          // icon, existing selection should be cleared.
          if (!sm.getCheckSelectMode()) {
            sm.unselectAll();
          }
        }
        // Always enables check-select mode when the selection is updated by
        // Ctrl+Click or Click on an item's icon.
        sm.setCheckSelectMode(true);

        // Toggle the current one and make it anchor index.
        const name = this.filesView.getItemLabel(index);
        const msgId = sm.getIndexSelected(index) ?
            'SELECTION_REMOVE_SINGLE_ENTRY' :
            'SELECTION_ADD_SINGLE_ENTRY';
        a11y.speakA11yMessage(strf(msgId, name));
        sm.setIndexSelected(index, !sm.getIndexSelected(index));
        sm.leadIndex = index;
        sm.anchorIndex = index;
      }
    } else if (e.shiftKey && anchorIndex !== -1 && anchorIndex !== index) {
      // Shift is done in mousedown.
      if (isDown) {
        sm.unselectAll();
        sm.leadIndex = index;
        if (sm.multiple) {
          sm.selectRange(anchorIndex, index);
          const nameStart = this.filesView.getItemLabel(anchorIndex);
          const nameEnd = this.filesView.getItemLabel(index);
          const count = Math.abs(index - anchorIndex) + 1;
          const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
          a11y.speakA11yMessage(msg);
        } else {
          sm.setIndexSelected(index, true);
        }
      }
    } else {
      // Right click for a context menu needs to not clear the selection.
      const isRightClick = e.button === 2;

      // If the index is selected this is handled in mouseup.
      const indexSelected = sm.getIndexSelected(index);
      if ((indexSelected && !isDown || !indexSelected && isDown) &&
          !(indexSelected && isRightClick)) {
        // 2) When non-checkmark area is clicked in check-select mode,
        // disable
        //    the check-select mode.
        if (sm.getCheckSelectMode()) {
          // Unselect all items once to ensure that the check-select mode is
          // terminated.
          sm.endChange();
          sm.unselectAll();
          sm.beginChange();
        }
        sm.selectedIndex = index;
      }
    }
  }
  sm.endChange();
}

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
 * @param e The keydown event.
 */
export function handleKeyDown(
    this: FileListSelectionController|FileGridSelectionController,
    e: KeyboardEvent) {
  const target = e.target as HTMLElement;
  const tagName = target.tagName;

  // If focus is in an input field of some kind, only handle navigation keys
  // that aren't likely to conflict with input interaction (e.g., text
  // editing, or changing the value of a checkbox or select).
  if (tagName === 'INPUT') {
    const inputType = (target as HTMLInputElement).type;
    // Just protect space (for toggling) for checkbox and radio.
    if (inputType === 'checkbox' || inputType === 'radio') {
      if (e.key === ' ') {
        return;
      }
      // Protect all but the most basic navigation commands in anything
      // else.
    } else if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') {
      return;
    }
  }
  // Similarly, don't interfere with select element handling.
  if (tagName === 'SELECT') {
    return;
  }

  const sm = this.selectionModel as FileListSelectionModel;
  assert(sm);
  let newIndex = -1;
  const leadIndex = sm.leadIndex;
  let prevent = true;

  const a11y = this.filesView.a11y!;

  // Ctrl/Meta+A. Use keyCode=65 to use the same shortcut key regardless of
  // keyboard layout.
  const pressedKeyA = e.keyCode === 65 || e.key === 'a';
  if (sm.multiple && pressedKeyA && e.ctrlKey) {
    a11y.speakA11yMessage(str('SELECTION_ALL_ENTRIES'));
    sm.setCheckSelectMode(true);
    sm.selectAll();
    e.preventDefault();
    return;
  }

  // Esc
  if (e.key === 'Escape' && !e.ctrlKey && !e.shiftKey) {
    a11y.speakA11yMessage(str('SELECTION_CANCELLATION'));
    sm.unselectAll();
    e.preventDefault();
    return;
  }

  // Space: Note ChromeOS and ChromeOS on Linux can generate KeyDown Space
  // events differently the |key| attribute might be set to 'Unidentified'.
  if (e.code === 'Space' || e.key === ' ') {
    if (leadIndex !== -1) {
      const selected = sm.getIndexSelected(leadIndex);
      if (e.ctrlKey) {
        sm.beginChange();

        // Force selecting if it's the first item selected, otherwise flip
        // the "selected" status.
        if (selected && sm.selectedIndexes.length === 1 &&
            !sm.getCheckSelectMode()) {
          // It needs to go back/forth to trigger the 'change' event.
          sm.setIndexSelected(leadIndex, false);
          sm.setIndexSelected(leadIndex, true);
          const name = this.filesView.getItemLabel(leadIndex);
          a11y.speakA11yMessage(strf('SELECTION_SINGLE_ENTRY', name));
        } else {
          // Toggle the current one and make it anchor index.
          sm.setIndexSelected(leadIndex, !selected);
          const name = this.filesView.getItemLabel(leadIndex);
          const msgId = selected ? 'SELECTION_REMOVE_SINGLE_ENTRY' :
                                   'SELECTION_ADD_SINGLE_ENTRY';
          a11y.speakA11yMessage(strf(msgId, name));
        }

        // Force check-select, FileListSelectionModel.onChangeEvent_ resets
        // it if needed.
        sm.setCheckSelectMode(true);
        sm.endChange();

        // Prevents space to opening quickview.
        e.stopPropagation();
        e.preventDefault();
        return;
      }
    }
  }

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
    if (e.shiftKey) {
      const anchorIndex = sm.anchorIndex;
      if (sm.multiple) {
        sm.unselectAll();
      }
      if (anchorIndex === -1) {
        sm.setIndexSelected(newIndex, true);
        sm.anchorIndex = newIndex;
      } else {
        const nameStart = this.filesView.getItemLabel(anchorIndex);
        const nameEnd = this.filesView.getItemLabel(newIndex);
        const count = Math.abs(newIndex - anchorIndex) + 1;
        const msg = strf('SELECTION_ADD_RANGE', count, nameStart, nameEnd);
        a11y.speakA11yMessage(msg);
        sm.selectRange(anchorIndex, newIndex);
      }
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
      sm.setIndexSelected(newIndex, true);
      sm.anchorIndex = newIndex;
    }

    sm.endChange();

    if (prevent) {
      e.preventDefault();
    }
  }
}

/**
 * Focus on the file list that contains the event target.
 * @param event the touch event.
 */
export function focusParentList(event: Event) {
  let element = event.target;
  while (element && !(element instanceof List)) {
    element = (element as HTMLElement).parentElement;
  }
  if (element) {
    element.focus();
  }
}

/**
 * Update the item's inline status when it's restored from List's cache..
 * @param restoredItem Item being restored from the List cache.
 * @param dataModel Data model corresponding to the item.
 * @param metadataModel Cache to retrieve metadata.
 */
export function updateCacheItemInlineStatus(
    restoredItem: ListItem, dataModel: ArrayDataModel|null,
    metadataModel: MetadataModel) {
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
  ])[0]!;

  updateInlineStatus(restoredItem, metadata);
}

/**
 * Update status icon for file or directory entry.
 * @param li The grid item.
 * @param metadata Metadata.
 */
export function updateInlineStatus(
    li: HTMLLIElement, metadata: null|MetadataItem) {
  const inlineStatus = li.querySelector('xf-inline-status');

  if (!metadata || !inlineStatus) {
    return;
  }

  const {
    pinned,
    availableOffline,
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

  let actualSyncStatus = syncStatus;
  let actualProgress = progress;
  // Force sync status as completed if it has been less than 300ms since
  // the file has completed syncing.
  if (Date.now() - (syncCompletedTime ?? 0) < 300) {
    actualSyncStatus = chrome.fileManagerPrivate.SyncStatus.COMPLETED;
    actualProgress = 1;
  }
  inlineStatus.setAttribute('sync-status', String(actualSyncStatus));
  inlineStatus.setAttribute('progress', String(actualProgress));
}
