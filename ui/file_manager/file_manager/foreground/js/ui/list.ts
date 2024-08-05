// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {ArrayDataModel, ChangeEvent, PermutationEvent} from '../../../common/js/array_data_model.js';
import {boolAttrSetter, crInjectTypeAndInit, type PropertyChangeEvent} from '../../../common/js/cr_ui.js';
import {isNullOrUndefined} from '../../../common/js/util.js';

import type {ListItem} from './list_item.js';
import {createListItem} from './list_item.js';
import {ListSelectionController} from './list_selection_controller.js';
import {ListSelectionModel, type SelectionChangeEvent} from './list_selection_model.js';
import type {ListSingleSelectionModel} from './list_single_selection_model.js';

/**
 * @fileoverview This implements a list control.
 */

interface Size {
  height: number;
  marginBottom: number;
  marginLeft: number;
  marginRight: number;
  marginTop: number;
  width: number;
}

type EventHandler = (event: Event) => void;

/**
 * Whether a mouse event is inside the element viewport. This will return
 * false if the mouseevent was generated over a border or a scrollbar.
 * @param el The element to test the event with.
 * @param e The mouse event.
 */
function inViewport(el: HTMLElement, e: MouseEvent): boolean {
  const rect = el.getBoundingClientRect();
  const x = e.clientX;
  const y = e.clientY;
  return x >= rect.left + el.clientLeft &&
      x < rect.left + el.clientLeft + el.clientWidth &&
      y >= rect.top + el.clientTop &&
      y < rect.top + el.clientTop + el.clientHeight;
}

function getComputedStyle(el: HTMLElement) {
  return el.ownerDocument?.defaultView?.getComputedStyle(el);
}

export function createList(): List {
  const el = document.createElement('list') as List;
  crInjectTypeAndInit(el, List);
  return el;
}

/**
 * Creates a new list element.
 */
export abstract class List extends HTMLUListElement {
  /**
   * Measured size of list items. This is lazily calculated the first time it
   * is needed. Note that lead item is allowed to have a different height, to
   * accommodate lists where a single item at a time can be expanded to show
   * more detail.
   */
  private measured_: Size|null = null;

  /**
   * Whether or not the list is auto-expanding. If true, the list resizes
   * its height to accommodate all children.
   */
  private autoExpands_ = false;

  private firstIndex_ = 0;
  private lastIndex_ = 0;
  private pinnedItem_: ListItem|null = null;

  /**
   * Whether or not the rows on list have various heights. If true, all the
   * rows have the same fixed height. Otherwise, each row resizes its height
   * to accommodate all contents.
   */
  private fixedHeight_ = true;

  /**
   * Whether or not the list view has a blank space below the last row.
   */
  private remainingSpace_ = true;

  /**
   * Function used to create grid items.
   */
  protected itemConstructor_: (...args: any[]) => ListItem = createListItem;

  private dataModel_: ArrayDataModel|null = null;
  private selectionModel_: ListSelectionModel|ListSingleSelectionModel|null =
      null;
  private selectionController_: ListSelectionController|null = null;

  /**
   * Cached item for measuring the default item size by measureItem().
   */
  private cachedMeasuredItem_: ListItem|null = null;

  /**
   * Maps the index to the ListItem.
   */
  private cachedItems_: Record<number, ListItem> = {};

  /**
   * Maps the index to the ListItem's height.
   */
  private cachedItemHeights_: Record<number, number> = {};

  private boundHandleDataModelPermuted_: EventHandler|null = null;
  private boundHandleDataModelChange_: EventHandler|null = null;
  private boundHandleOnChange_: EventListenerOrEventListenerObject|null = null;
  private boundHandleLeadChange_: EventHandler|null = null;
  protected beforeFiller_: HTMLElement|null = null;
  protected afterFiller_: HTMLElement|null = null;
  /** Managed by DragSelector */
  cachedBounds: DOMRect|null = null;

  /**
   * Function used to create grid items.
   */
  get itemConstructor() {
    return this.itemConstructor_;
  }

  set itemConstructor(func) {
    if (func !== this.itemConstructor_) {
      this.itemConstructor_ = func;
      this.cachedItems_ = {};
      this.redraw();
    }
  }

  /**
   * The data model driving the list.
   */
  set dataModel(dataModel: ArrayDataModel|null) {
    if (this.dataModel_ === dataModel) {
      return;
    }

    if (!this.boundHandleDataModelPermuted_) {
      this.boundHandleDataModelPermuted_ =
          this.handleDataModelPermuted_.bind(this) as EventListener;
      this.boundHandleDataModelChange_ =
          this.handleDataModelChange_.bind(this) as EventListener;
    }

    if (this.dataModel_) {
      this.dataModel_.removeEventListener(
          'permuted', this.boundHandleDataModelPermuted_);
      this.dataModel_.removeEventListener(
          'change', this.boundHandleDataModelChange_);
    }

    this.dataModel_ = dataModel;

    this.cachedItems_ = {};
    this.cachedItemHeights_ = {};
    this.selectionModel?.clear();
    if (dataModel) {
      this.selectionModel?.adjustLength(dataModel.length);
    }

    if (this.dataModel_) {
      this.dataModel_.addEventListener(
          'permuted', this.boundHandleDataModelPermuted_);
      this.dataModel_.addEventListener(
          'change', this.boundHandleDataModelChange_);
    }

    this.redraw();
  }

  get dataModel(): ArrayDataModel|null {
    return this.dataModel_;
  }

  /**
   * The selection model to use.
   */
  get selectionModel(): ListSelectionModel|ListSingleSelectionModel|null {
    return this.selectionModel_;
  }
  set selectionModel(sm: ListSelectionModel|ListSingleSelectionModel) {
    const oldSm = this.selectionModel_;
    if (oldSm === sm) {
      return;
    }

    if (!this.boundHandleOnChange_) {
      this.boundHandleOnChange_ =
          this.handleOnChange_.bind(this) as EventListenerOrEventListenerObject;
      this.boundHandleLeadChange_ = this.handleLeadChange.bind(this);
    }

    if (oldSm) {
      oldSm.removeEventListener('change', this.boundHandleOnChange_!);
      oldSm.removeEventListener('leadIndexChange', this.boundHandleLeadChange_);
    }

    this.selectionModel_ = sm;
    this.selectionController_ = this.createSelectionController(sm);

    if (sm) {
      sm.addEventListener('change', this.boundHandleOnChange_!);
      sm.addEventListener('leadIndexChange', this.boundHandleLeadChange_);
    }
  }

  /**
   * Whether or not the list auto-expands.
   */
  get autoExpands() {
    return this.autoExpands_;
  }

  set autoExpands(autoExpands: boolean) {
    if (this.autoExpands_ === autoExpands) {
      return;
    }
    this.autoExpands_ = autoExpands;
    this.redraw();
  }

  /**
   * Whether or not the rows on list have various heights.
   */
  get fixedHeight(): boolean {
    return this.fixedHeight_;
  }
  set fixedHeight(fixedHeight: boolean) {
    if (this.fixedHeight_ === fixedHeight) {
      return;
    }
    this.fixedHeight_ = fixedHeight;
    this.redraw();
  }

  /**
   * Convenience alias for selectionModel.selectedItem
   */
  get selectedItem(): unknown|null {
    const dataModel = this.dataModel;
    if (dataModel) {
      const index = this.selectionModel!.selectedIndex;
      if (index !== -1) {
        return dataModel.item(index) ?? null;
      }
    }
    return null;
  }

  set selectedItem(selectedItem: unknown) {
    const dataModel = this.dataModel;
    if (dataModel) {
      const index = this.dataModel!.indexOf(selectedItem);
      this.selectionModel!.selectedIndex = index;
    }
  }

  /**
   * Convenience alias for selectionModel.selectedItems
   */
  get selectedItems(): unknown[] {
    const indexes = this.selectionModel!.selectedIndexes;
    const dataModel = this.dataModel;
    if (dataModel) {
      return indexes
          .map(i => dataModel.item(i))
          // b/307500990 somehow this was getting invalid indexes.
          .filter(item => item !== undefined);
    }
    return [];
  }

  /**
   * The HTML elements representing the items.
   */
  get items(): HTMLElement[] {
    return Array.prototype.filter.call(this.children, this.isItem, this);
  }

  /**
   * Returns true if the child is a list item. Subclasses may override this
   * to filter out certain elements.
   */
  isItem(child: Node): boolean {
    return child.nodeType === Node.ELEMENT_NODE &&
        child !== this.beforeFiller_ && child !== this.afterFiller_;
  }


  protected batchCount_ = 0;

  /**
   * When making a lot of updates to the list, the code could be wrapped in
   * the startBatchUpdates and finishBatchUpdates to increase performance.
   * Be sure that the code will not return without calling endBatchUpdates
   * or the list will not be correctly updated.
   */
  startBatchUpdates() {
    this.batchCount_++;
  }


  /**
   * See startBatchUpdates.
   */
  endBatchUpdates() {
    this.batchCount_--;
    if (this.batchCount_ === 0) {
      this.redraw();
    }
  }

  /**
   * Initializes the element.
   */
  initialize() {
    // Add fillers.
    this.beforeFiller_ = this.ownerDocument.createElement('div');
    this.afterFiller_ = this.ownerDocument.createElement('div');
    this.beforeFiller_.className = 'spacer';
    this.afterFiller_.className = 'spacer';
    this.textContent = '';
    this.appendChild(this.beforeFiller_);
    this.appendChild(this.afterFiller_);
    this.autoExpands_ = false;
    this.fixedHeight_ = true;
    this.remainingSpace_ = true;
    this.batchCount_ = 0;
    this.itemConstructor_ = (label: string) => {
      const item = createListItem();
      item.label = label;
      return item;
    };

    const length = this.dataModel ? this.dataModel.length : 0;
    this.selectionModel = new ListSelectionModel(length);

    this.addEventListener('dblclick', this.handleDoubleClick_);
    this.addEventListener('mousedown', this.handleMouseDown_.bind(this));
    this.addEventListener('dragstart', this.handleDragStart_.bind(this), true);
    this.addEventListener('mouseup', this.handlePointerDownUp_);
    this.addEventListener('keydown', this.handleKeyDown);
    this.addEventListener('focus', this.handleElementFocus_, true);
    this.addEventListener('blur', this.handleElementBlur_, true);
    this.addEventListener('scroll', this.handleScroll.bind(this));
    this.addEventListener('touchstart', this.handleTouchEvents_);
    this.addEventListener('touchmove', this.handleTouchEvents_);
    this.addEventListener('touchend', this.handleTouchEvents_);
    this.addEventListener('touchcancel', this.handleTouchEvents_);
    this.setAttribute('role', 'list');

    // Make list focusable
    if (!this.hasAttribute('tabindex')) {
      this.tabIndex = 0;
    }
  }

  /**
   * @param item The list item to measure.
   * @return The height of the given item. If the fixed height on CSS
   * is set by 'px', uses that value as height. Otherwise, measures the
   * size.
   */
  private measureItemHeight_(item?: ListItem): number {
    return this.measureItem(item).height;
  }

  /**
   * The height of default item, measuring it if necessary.
   */
  protected getDefaultItemHeight_() {
    return this.getDefaultItemSize_().height;
  }

  /**
   * @param index The index of the item.
   * @return The height of the item, measuring it if necessary.
   */
  protected getItemHeightByIndex_(index: number): number {
    // If |this.fixedHeight_| is true, all the rows have same default height.
    if (this.fixedHeight_) {
      return this.getDefaultItemHeight_();
    }

    if (this.cachedItemHeights_[index]) {
      return this.cachedItemHeights_[index]!;
    }

    const item = this.getListItemByIndex(index);
    if (item) {
      const h = this.measureItemHeight_(item);
      this.cachedItemHeights_[index] = h;
      return h;
    }
    return this.getDefaultItemHeight_();
  }

  /**
   * The height and width of default item, measuring it if necessary.
   */
  protected getDefaultItemSize_(): Size {
    if (!this.measured_ || !this.measured_.height) {
      this.measured_ = this.measureItem();
    }
    return this.measured_;
  }

  /**
   * Creates an item (dataModel.item(0)) and measures its height. The item
   * is cached instead of creating a new one every time..
   * @param {ListItem=} item The list item to use to do the measuring. If this
   *     is not provided an item will be created based on the first value in the
   *     model.
   * @return  The height and width of the item, taking margins into account, and
   *     the top, bottom, left and right margins themselves.
   */
  measureItem(item?: ListItem): Size {
    const dataModel = this.dataModel;
    if (!dataModel || !dataModel.length) {
      return {
        height: 0,
        marginTop: 0,
        marginBottom: 0,
        width: 0,
        marginLeft: 0,
        marginRight: 0,
      };
    }
    const measuredItem =
        item || this.cachedMeasuredItem_ || this.createItem(dataModel.item(0));
    if (!item) {
      this.cachedMeasuredItem_ = measuredItem;
      this.appendChild(measuredItem);
    }

    const rect = measuredItem.getBoundingClientRect();
    const cs = getComputedStyle(measuredItem);
    const mt = parseFloat(cs?.marginTop ?? '');
    const mb = parseFloat(cs?.marginBottom ?? '');
    const ml = parseFloat(cs?.marginLeft ?? '');
    const mr = parseFloat(cs?.marginRight ?? '');
    let h = rect.height;
    let w = rect.width;
    let mh = 0;
    let mv = 0;

    // Handle margin collapsing.
    if (mt < 0 && mb < 0) {
      mv = Math.min(mt, mb);
    } else if (mt >= 0 && mb >= 0) {
      mv = Math.max(mt, mb);
    } else {
      mv = mt + mb;
    }
    h += mv;

    if (ml < 0 && mr < 0) {
      mh = Math.min(ml, mr);
    } else if (ml >= 0 && mr >= 0) {
      mh = Math.max(ml, mr);
    } else {
      mh = ml + mr;
    }
    w += mh;

    if (!item) {
      this.removeChild(measuredItem);
    }
    return {
      height: Math.max(0, h),
      marginTop: mt,
      marginBottom: mb,
      width: Math.max(0, w),
      marginLeft: ml,
      marginRight: mr,
    };
  }

  /**
   * Callback for the double click event.
   * @param e The mouse event object.
   */
  private handleDoubleClick_(e: MouseEvent) {
    if (this.disabled) {
      return;
    }

    const target: HTMLElement|null = e.target as HTMLElement;

    const ancestor = this.getListItemAncestor(target);
    let index = -1;
    if (ancestor) {
      index = this.getIndexOfListItem(ancestor);
      this.activateItemAtIndex(index);
    }

    const sm = this.selectionModel;
    const indexSelected = sm?.getIndexSelected(index);
    if (!indexSelected) {
      this.handlePointerDownUp_(e);
    }
  }

  /**
   * Callback for mousedown and mouseup events.
   * @param e The mouse event object.
   */
  private handlePointerDownUp_(e: MouseEvent) {
    if (this.disabled) {
      return;
    }

    let target: HTMLElement|null = e.target as HTMLElement;

    // If the target was this element we need to make sure that the user did
    // not click on a border or a scrollbar.
    if (target === this) {
      if (inViewport(target, e)) {
        this.selectionController_!.handlePointerDownUp(e, -1);
      }
      return;
    }

    target = this.getListItemAncestor(target);
    if (!target) {
      return;
    }

    const index = this.getIndexOfListItem(target as ListItem);
    this.selectionController_!.handlePointerDownUp(e, index);
  }

  /**
   * Called when an element in the list is focused. Marks the list as having
   * a focused element, and dispatches an event if it didn't have focus.
   * @param e The focus event.
   */
  private handleElementFocus_(_e: FocusEvent) {
    if (!this.hasElementFocus) {
      this.hasElementFocus = true;
    }
  }

  /**
   * Called when an element in the list is blurred. If focus moves
   * outside the list, marks the list as no longer having focus and
   * dispatches an event.
   */
  private handleElementBlur_(e: FocusEvent) {
    if (!this.contains(e.relatedTarget as HTMLElement)) {
      this.hasElementFocus = false;
    }
  }

  /**
   * Returns the list item element containing the given element, or null if
   * it doesn't belong to any list item element.
   * @param element The element.
   * @return The list item containing `element`, or null.
   */
  getListItemAncestor(element: HTMLElement|null): ListItem|null {
    let container: ParentNode|null = element;
    while (container && container.parentNode !== this) {
      container = container.parentNode;
    }

    return (container instanceof HTMLLIElement ? container as ListItem : null);
  }

  /**
   * Handle a keydown event.
   */
  handleKeyDown(e: KeyboardEvent) {
    if (!this.disabled) {
      this.selectionController_?.handleKeyDown(e);
    }
  }

  /**
   * Handle a scroll event.
   */
  handleScroll(_e: Event) {
    requestAnimationFrame(this.redraw.bind(this));
  }

  /**
   * Handle touchmove/touchcancel events.
   */
  private handleTouchEvents_(e: Event) {
    if (this.disabled) {
      return;
    }

    let target: HTMLElement|null = e.target as HTMLElement;

    if (target === this) {
      // Unlike the mouse events, we don't check if the touch is inside the
      // viewport because of these reasons:
      // - The scrollbars do not interact with touch.
      // - touch* events are not sent to this element when tapping or
      //   dragging window borders by touch.
      this.selectionController_!.handleTouchEvents(e, -1);
      return;
    }

    target = this.getListItemAncestor(target);
    if (!target) {
      return;
    }

    const index = this.getIndexOfListItem(target as ListItem);
    this.selectionController_!.handleTouchEvents(e, index);
  }

  /**
   * Callback from the selection model. We dispatch {@code change} events
   * when the selection changes.
   * @param event Event with change info.
   * @private
   */
  private handleOnChange_(event: SelectionChangeEvent) {
    const changes = event.detail.changes || [];
    for (const change of changes) {
      const listItem = this.getListItemByIndex(change.index);
      if (listItem) {
        listItem.selected = change.selected;
        if (change.selected) {
          listItem.setAttribute('aria-posinset', String(change.index + 1));
          listItem.setAttribute('aria-setsize', String(this.dataModel!.length));
        } else {
          listItem.removeAttribute('aria-posinset');
          listItem.removeAttribute('aria-setsize');
        }
      }
    }

    dispatchSimpleEvent(this, 'change');
  }

  /**
   * Handles a change of the lead item from the selection model.
   * @param event The property change event.
   */
  protected handleLeadChange(event: Event) {
    const e = event as PropertyChangeEvent<number>;
    let element;
    if (e.oldValue !== -1) {
      if ((element = this.getListItemByIndex(e.oldValue!))) {
        element.lead = false;
      }
    }

    if (e.newValue !== -1) {
      if ((element = this.getListItemByIndex(e.newValue!))) {
        element.lead = true;
      }
      if (e.oldValue !== e.newValue) {
        if (element) {
          this.setAttribute('aria-activedescendant', element.id);
        }
        this.scrollIndexIntoView(e.newValue!);
        // If the lead item has a different height than other items, then we
        // may run into a problem that requires a second attempt to scroll
        // it into view. The first scroll attempt will trigger a redraw,
        // which will clear out the list and repopulate it with new items.
        // During the redraw, the list may shrink temporarily, which if the
        // lead item is the last item, will move the scrollTop up since it
        // cannot extend beyond the end of the list. (Sadly, being scrolled to
        // the bottom of the list is not "sticky.") So, we set a timeout to
        // rescroll the list after this all gets sorted out. This is perhaps
        // not the most elegant solution, but no others seem obvious.
        setTimeout(() => {
          this.scrollIndexIntoView(e.newValue!);
        });
      }
    } else {
      this.removeAttribute('aria-activedescendant');
    }
  }

  /**
   * This handles data model 'permuted' event.
   * this event is dispatched as a part of sort or splice.
   * We need to
   *  - adjust the cache.
   *  - adjust selection.
   *  - redraw. (called in this.endBatchUpdates())
   *  It is important that the cache adjustment happens before selection
   * model adjustments.
   * @param event The 'permuted' event.
   */
  private handleDataModelPermuted_(event: PermutationEvent) {
    const newCachedItems: Record<number, ListItem> = {};
    for (const index in this.cachedItems_) {
      if (event.detail.permutation[index] !== -1) {
        const newIndex = event.detail.permutation[index]!;
        newCachedItems[newIndex] = this.cachedItems_[index]!;
        newCachedItems[newIndex]!.listIndex = newIndex;
      }
    }
    this.cachedItems_ = newCachedItems;
    this.pinnedItem_ = null;

    const newCachedItemHeights: Record<number, number> = {};
    for (const index in this.cachedItemHeights_) {
      if (event.detail.permutation[index] !== -1) {
        newCachedItemHeights[event.detail.permutation[index]!] =
            this.cachedItemHeights_[index]!;
      }
    }
    this.cachedItemHeights_ = newCachedItemHeights;

    this.startBatchUpdates();

    assert(this.selectionModel);
    const sm = this.selectionModel;
    sm.adjustLength(event.detail.newLength);
    sm.adjustToReordering(event.detail.permutation);

    this.endBatchUpdates();
  }

  private handleDataModelChange_(event: ChangeEvent) {
    if (isNullOrUndefined(event.detail.index)) {
      return;
    }
    const eventIndex = event.detail.index;
    delete this.cachedItems_[eventIndex];
    delete this.cachedItemHeights_[eventIndex];
    this.cachedMeasuredItem_ = null;

    if (eventIndex >= this.firstIndex_ &&
        (eventIndex < this.lastIndex_ || this.remainingSpace_)) {
      this.redraw();
    }
  }

  /**
   * @param index The index of the item.
   * @return The top position of the item inside the list.
   */
  getItemTop(index: number): number {
    if (this.fixedHeight_) {
      const itemHeight = this.getDefaultItemHeight_();
      return index * itemHeight;
    } else {
      this.ensureAllItemSizesInCache();
      let top = 0;
      for (let i = 0; i < index; i++) {
        top += this.getItemHeightByIndex_(i);
      }
      return top;
    }
  }

  /**
   * @param index The index of the item.
   * @return The row of the item. May vary in the case of multiple columns.
   */
  getItemRow(index: number): number {
    return index;
  }

  /**
   * @param row The row.
   * @return The index of the first item in the row.
   */
  getFirstItemInRow(row: number): number {
    return row;
  }

  /**
   * Ensures that a given index is inside the viewport.
   * @param index The index of the item to scroll into view.
   */
  scrollIndexIntoView(index: number) {
    const dataModel = this.dataModel;
    if (!dataModel || index < 0 || index >= dataModel.length) {
      return;
    }

    const itemHeight = this.getItemHeightByIndex_(index);
    const scrollTop = this.scrollTop;
    const top = this.getItemTop(index);
    const clientHeight = this.clientHeight;

    const cs = getComputedStyle(this);
    assert(cs);
    const paddingY =
        parseInt(cs.paddingTop, 10) + parseInt(cs.paddingBottom, 10);
    const availableHeight = clientHeight - paddingY;

    const self = this;
    // Function to adjust the tops of viewport and row.
    function scrollToAdjustTop() {
      self.scrollTop = top;
    }
    // Function to adjust the bottoms of viewport and row.
    function scrollToAdjustBottom() {
      self.scrollTop = top + itemHeight - availableHeight;
    }

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
   * @return The rect to use for the context menu.
   */
  getRectForContextMenu(): ClientRect {
    assert(this.selectionModel);
    const index = this.selectionModel.selectedIndex;
    const el = this.getListItemByIndex(index);
    if (el) {
      return el.getBoundingClientRect();
    }
    return this.getBoundingClientRect();
  }

  /**
   * Takes a value from the data model and finds the associated list item.
   * @param value The value in the data model that we want to get the list item
   *     for.
   * @return The first found list item or null if not found.
   */
  getListItem(value: unknown): ListItem|null {
    const dataModel = this.dataModel;
    if (dataModel) {
      const index = dataModel.indexOf(value);
      return this.getListItemByIndex(index);
    }
    return null;
  }

  /**
   * Find the list item element at the given index.
   * @param index The index of the list item to get.
   * @return The found list item or null if not found.
   */
  getListItemByIndex(index: number): ListItem|null {
    return this.cachedItems_[index] || null;
  }

  /**
   * Find the index of the given list item element.
   * @return  The index of the list item, or -1 if not found.
   */
  getIndexOfListItem(item: ListItem): number {
    const index = item.listIndex;
    if (this.cachedItems_[index] === item) {
      return index;
    }
    return -1;
  }

  /**
   * Creates a new list item.
   * @param label The value to use for the item.
   * @return The newly created list item.
   */
  createItem(label: string): ListItem {
    const item = this.itemConstructor_(label);
    return item;
  }

  /**
   * Creates the selection controller to use internally.
   * @param sm The underlying selection model.
   * @return The newly created selection controller.
   */
  createSelectionController(sm: ListSelectionModel|
                            ListSingleSelectionModel): ListSelectionController {
    return new ListSelectionController(sm);
  }

  /**
   * Return the heights (in pixels) of the top of the given item index
   * within the list, and the height of the given item itself, accounting
   * for the possibility that the lead item may be a different height.
   * @param index The index to find the top height of.
   * @return  The heights for the given index.
   */
  getHeightsForIndex(index: number): {top: number, height: number} {
    const itemHeight = this.getItemHeightByIndex_(index);
    const top = this.getItemTop(index);
    return {top: top, height: itemHeight};
  }

  /**
   * Find the index of the list item containing the given y offset (measured
   * in pixels from the top) within the list. In the case of multiple
   * columns, returns the first index in the row.
   * @param offset The y offset in pixels to get the index of.
   * @return The index of the list item. Returns the list size if given offset
   *     exceeds the height of list.
   */
  protected getIndexForListOffset_(offset: number): number {
    const itemHeight = this.getDefaultItemHeight_();
    assert(this.dataModel);
    if (!itemHeight) {
      return this.dataModel.length;
    }

    if (this.fixedHeight_) {
      return this.getFirstItemInRow(Math.floor(offset / itemHeight));
    }

    // If offset exceeds the height of list.
    let lastHeight = 0;
    if (this.dataModel.length) {
      const h = this.getHeightsForIndex(this.dataModel.length - 1);
      lastHeight = h.top + h.height;
    }
    if (lastHeight < offset) {
      return this.dataModel.length;
    }

    // Estimates index.
    let estimatedIndex =
        Math.min(Math.floor(offset / itemHeight), this.dataModel.length - 1);
    const isIncrementing = this.getItemTop(estimatedIndex) < offset;

    // Searches the correct index.
    do {
      const heights = this.getHeightsForIndex(estimatedIndex);
      const top = heights.top;
      const height = heights.height;

      if (top <= offset && offset <= (top + height)) {
        break;
      }

      isIncrementing ? ++estimatedIndex : --estimatedIndex;
    } while (0 < estimatedIndex && estimatedIndex < this.dataModel.length);

    return estimatedIndex;
  }

  /**
   * Return the number of items that occupy the range of heights between
   * the top of the start item and the end offset.
   * @param startIndex The index of the first visible item.
   * @param  endOffset The y offset in pixels of the end of the list.
   */
  protected countItemsInRange_(startIndex: number, endOffset: number): number {
    const endIndex = this.getIndexForListOffset_(endOffset);
    return endIndex - startIndex + 1;
  }

  /**
   * Calculates the number of items fitting in the given viewport.
   * @param scrollTop The scroll top position.
   * @param clientHeight The height of viewport.
   * @return The index of first item in view port, The number of items, The item
   *     past the last.
   */
  getItemsInViewPort(scrollTop: number, clientHeight: number):
      {first: number, length: number, last: number} {
    if (this.autoExpands_) {
      return {
        first: 0,
        length: this.dataModel?.length ?? 0,
        last: this.dataModel?.length ?? 0,
      };
    } else {
      const firstIndex = this.getIndexForListOffset_(scrollTop);
      const lastIndex = this.getIndexForListOffset_(scrollTop + clientHeight);

      return {
        first: firstIndex,
        length: lastIndex - firstIndex + 1,
        last: lastIndex + 1,
      };
    }
  }

  /**
   * Merges list items currently existing in the list with items in the
   * range [firstIndex, lastIndex). Removes or adds items if needed. Doesn't
   * delete {@code this.pinnedItem_} if it is present (instead hides it if
   * it is out of the range).
   * @param firstIndex The index of first item, inclusively.
   * @param lastIndex The index of last item, exclusively.
   */
  mergeItems(firstIndex: number, lastIndex: number) {
    let currentIndex = firstIndex;

    const insert = () => {
      assert(this.dataModel);
      const dataItem = this.dataModel.item(currentIndex);
      const cachedCurrentItem = this.cachedItems_[currentIndex];
      if (cachedCurrentItem) {
        // Emit synthetic event with cached item that is about to be restored.
        this.dispatchEvent(new CustomEvent('cachedItemRestored', {
          detail: cachedCurrentItem,
        }));
      }
      const newItem: ListItem = cachedCurrentItem || this.createItem(dataItem);
      newItem.listIndex = currentIndex;
      this.cachedItems_[currentIndex] = newItem;
      this.insertBefore(newItem, item);
      currentIndex++;
    };

    const remove = () => {
      const next = item.nextSibling as ListItem;
      if (item !== this.pinnedItem_) {
        this.removeChild(item);
      }
      item = next;
    };

    let item: ListItem;
    for (item = this.beforeFiller_?.nextSibling as ListItem;
         item !== this.afterFiller_ && currentIndex < lastIndex;) {
      if (!this.isItem(item)) {
        item = item.nextSibling as ListItem;
        continue;
      }

      const index = item.listIndex;
      if (this.cachedItems_[index] !== item || index < currentIndex) {
        remove();
      } else if (index === currentIndex) {
        this.cachedItems_[currentIndex] = item;
        item = item.nextSibling as ListItem;
        currentIndex++;
      } else {  // index > currentIndex
        insert();
      }
    }

    while (item !== this.afterFiller_) {
      if (this.isItem(item)) {
        remove();
      } else {
        item = item.nextSibling as ListItem;
      }
    }

    if (this.pinnedItem_) {
      const index = this.pinnedItem_.listIndex;
      this.pinnedItem_.hidden = index < firstIndex || index >= lastIndex;
      this.cachedItems_[index] = this.pinnedItem_;
      if (index >= lastIndex) {
        item = this.pinnedItem_;
      }  // Insert new items before this one.
    }

    while (currentIndex < lastIndex) {
      insert();
    }
  }

  /**
   * Ensures that all the item sizes in the list have been already cached.
   */
  ensureAllItemSizesInCache() {
    const measuringIndexes: number[] = [];
    const isElementAppended = [];
    assert(this.dataModel);
    for (let y = 0; y < this.dataModel.length; y++) {
      if (!this.cachedItemHeights_[y]) {
        measuringIndexes.push(y);
        isElementAppended.push(false);
      }
    }

    const measuringItems = [];
    // Adds temporary elements.
    for (let y = 0; y < measuringIndexes.length; y++) {
      const index = measuringIndexes[y];
      assert(index);
      const dataItem = this.dataModel.item(index);
      const listItem = this.cachedItems_[index] || this.createItem(dataItem);
      listItem.listIndex = index;

      // If `listItems` is not on the list, appends it to the list and sets
      // the flag.
      if (!listItem.parentNode) {
        this.appendChild(listItem);
        isElementAppended[y] = true;
      }

      this.cachedItems_[index] = listItem;
      measuringItems.push(listItem);
    }

    // All mesurings must be placed after adding all the elements, to prevent
    // performance reducing.
    for (let y = 0; y < measuringIndexes.length; y++) {
      const index = measuringIndexes[y];
      assert(index);
      this.cachedItemHeights_[index] =
          this.measureItemHeight_(measuringItems[y]);
    }

    // Removes all the temporary elements.
    for (let y = 0; y < measuringIndexes.length; y++) {
      // If the list item has been appended above, removes it.
      if (isElementAppended[y]) {
        this.removeChild(measuringItems[y]!);
      }
    }
  }

  /**
   * Returns the height of after filler in the list.
   * @param lastIndex The index of item past the last in viewport.
   */
  getAfterFillerHeight(lastIndex: number): number {
    assert(this.dataModel);
    if (this.fixedHeight_) {
      const itemHeight = this.getDefaultItemHeight_();
      return (this.dataModel.length - lastIndex) * itemHeight;
    }

    let height = 0;
    for (let i = lastIndex; i < this.dataModel.length; i++) {
      height += this.getItemHeightByIndex_(i);
    }
    return height;
  }

  /**
   * Redraws the viewport.
   */
  redraw() {
    if (this.batchCount_ !== 0) {
      return;
    }

    const dataModel = this.dataModel;
    if (!dataModel || !this.autoExpands_ && this.clientHeight === 0) {
      this.cachedItems_ = {};
      this.firstIndex_ = 0;
      this.lastIndex_ = 0;
      this.remainingSpace_ = this.clientHeight !== 0;
      this.mergeItems(0, 0);
      return;
    }

    // Save the previous positions before any manipulation of elements.
    const scrollTop = this.scrollTop;
    const clientHeight = this.clientHeight;

    // Store all the item sizes into the cache in advance, to prevent
    // interleave measuring with mutating dom.
    if (!this.fixedHeight_) {
      this.ensureAllItemSizesInCache();
    }

    const itemsInViewPort = this.getItemsInViewPort(scrollTop, clientHeight);
    // Draws the hidden rows just above/below the viewport to prevent
    // flashing in scroll.
    const firstIndex =
        Math.max(0, Math.min(dataModel.length - 1, itemsInViewPort.first - 1));
    const lastIndex = Math.min(itemsInViewPort.last + 1, dataModel.length);

    const beforeFillerHeight =
        this.autoExpands ? 0 : this.getItemTop(firstIndex);
    const afterFillerHeight =
        this.autoExpands ? 0 : this.getAfterFillerHeight(lastIndex);

    this.beforeFiller_!.style.height = beforeFillerHeight + 'px';

    assert(this.selectionModel);
    const sm = this.selectionModel;
    const leadIndex = sm.leadIndex;

    // If the pinned item is hidden and it is not the lead item, then remove
    // it from cache. Note, that we restore the hidden status to false, since
    // the item is still in cache, and may be reused.
    if (this.pinnedItem_ && this.pinnedItem_ !== this.cachedItems_[leadIndex]) {
      if (this.pinnedItem_.hidden) {
        this.removeChild(this.pinnedItem_);
        this.pinnedItem_.hidden = false;
      }
      this.pinnedItem_ = null;
    }

    this.mergeItems(firstIndex, lastIndex);

    if (!this.pinnedItem_ && this.cachedItems_[leadIndex] &&
        this.cachedItems_[leadIndex]!.parentNode === this) {
      this.pinnedItem_ = this.cachedItems_[leadIndex] ?? null;
    }

    this.afterFiller_!.style.height = afterFillerHeight + 'px';

    // Restores the number of pixels scrolled, since it might be changed while
    // DOM operations.
    this.scrollTop = scrollTop;

    // We don't set the lead or selected properties until after adding all
    // items, in case they force relayout in response to these events.
    if (leadIndex !== -1 && this.cachedItems_[leadIndex]) {
      this.cachedItems_[leadIndex]!.lead = true;
    }
    for (let y = firstIndex; y < lastIndex; y++) {
      if (sm.getIndexSelected(y) !== this.cachedItems_[y]!.selected) {
        this.cachedItems_[y]!.selected = !this.cachedItems_[y]!.selected;
      }
    }

    this.firstIndex_ = firstIndex;
    this.lastIndex_ = lastIndex;
    this.remainingSpace_ = itemsInViewPort.last > dataModel.length;

    // Mesurings must be placed after adding all the elements, to prevent
    // performance reducing.
    if (!this.fixedHeight_) {
      for (let y = firstIndex; y < lastIndex; y++) {
        this.cachedItemHeights_[y] =
            this.measureItemHeight_(this.cachedItems_[y]);
      }
    }
  }

  /**
   * Restore the lead item that is present in the list but may be updated
   * in the data model (supposed to be used inside a batch update). Usually
   * such an item would be recreated in the redraw method. If reinsertion
   * is undesirable (for instance to prevent losing focus) the item may be
   * updated and restored. Assumed the listItem relates to the same data
   * item as the lead item in the begin of the batch update.
   *
   * @param leadItem Already existing lead item.
   */
  restoreLeadItem(leadItem: ListItem) {
    delete this.cachedItems_[leadItem.listIndex];

    leadItem.listIndex = this.selectionModel!.leadIndex;
    this.pinnedItem_ = this.cachedItems_[leadItem.listIndex] = leadItem;
  }

  /**
   * Invalidates list by removing cached items.
   */
  invalidate() {
    this.cachedItems_ = {};
  }

  /**
   * Redraws a single item.
   * @param index The row index to redraw.
   */
  redrawItem(index: number) {
    if (index >= this.firstIndex_ &&
        (index < this.lastIndex_ || this.remainingSpace_)) {
      delete this.cachedItems_[index];
      this.redraw();
    }
  }

  /**
   * Called when a list item is activated, currently only by a double click
   * event.
   * @param _index The index of the activated item.
   */
  activateItemAtIndex(_index: number) {}

  /**
   * Returns a ListItem for the leadIndex. If the item isn't present in the
   * list creates it and inserts to the list (may be invisible if it's out
   * of the visible range).
   *
   * Item returned from this method won't be removed until it remains a lead
   * item or till the data model changes (unlike other items that could be
   * removed when they go out of the visible range).
   */
  ensureLeadItemExists(): ListItem|null {
    const index = this.selectionModel!.leadIndex;
    if (index < 0) {
      return null;
    }
    const cachedItems = this.cachedItems_ || {};

    const item =
        cachedItems[index] || this.createItem(this.dataModel!.item(index));
    if (this.pinnedItem_ !== item && this.pinnedItem_ &&
        this.pinnedItem_.hidden) {
      this.removeChild(this.pinnedItem_);
    }
    this.pinnedItem_ = item;
    cachedItems[index] = item;
    item.listIndex = index;

    // 'Element'.;
    if (item.parentNode === this) {
      return item;
    }

    if (this.batchCount_ !== 0) {
      item.hidden = true;
    }

    // Item will get to the right place in redraw. Choose place to insert
    // reducing items reinsertion.
    if (index <= this.firstIndex_) {
      this.insertBefore(item, this.beforeFiller_?.nextSibling as Node);
    } else {
      this.insertBefore(item, this.afterFiller_);
    }
    this.redraw();
    return item;
  }

  /**
   * Starts drag selection by reacting 'dragstart' event.
   * @param event Event of dragstart.
   */
  startDragSelection(event: MouseEvent) {
    event.preventDefault();
    const border = document.createElement('div');
    border.className = 'drag-selection-border';
    const rect = this.getBoundingClientRect();
    const startX = event.clientX - rect.left + this.scrollLeft;
    const startY = event.clientY - rect.top + this.scrollTop;
    border.style.left = startX + 'px';
    border.style.top = startY + 'px';
    const onMouseMove = (event: MouseEvent) => {
      const inRect = this.getBoundingClientRect();
      const x = event.clientX - inRect.left + this.scrollLeft;
      const y = event.clientY - inRect.top + this.scrollTop;
      border.style.left = Math.min(startX, x) + 'px';
      border.style.top = Math.min(startY, y) + 'px';
      border.style.width = Math.abs(startX - x) + 'px';
      border.style.height = Math.abs(startY - y) + 'px';
    };
    const onMouseUp = () => {
      this.removeChild(border);
      document.removeEventListener('mousemove', onMouseMove, true);
      document.removeEventListener('mouseup', onMouseUp, true);
    };
    document.addEventListener('mousemove', onMouseMove, true);
    document.addEventListener('mouseup', onMouseUp, true);
    this.appendChild(border);
  }

  private handleMouseDown_(e: MouseEvent) {
    const target = e.target as HTMLElement;
    const listItem = this.getListItemAncestor(target);
    const wasSelected = listItem && listItem.selected;
    this.handlePointerDownUp_(e);

    if (e.defaultPrevented || e.button !== 0) {
      return;
    }

    // The following hack is required only if the listItem gets selected.
    if (!listItem || wasSelected || !listItem.selected) {
      return;
    }

    // If non-focusable area in a list item is clicked and the item still
    // contains the focused element, the item did a special focus handling
    // [1] and we should not focus on the list.
    //
    // [1] For example, clicking non-focusable area gives focus on the first
    // form control in the item.
    if (!containsFocusableElement(target, listItem) &&
        listItem.contains(listItem.ownerDocument.activeElement)) {
      e.preventDefault();
    }
  }

  /**
   * Dragstart event handler.
   * If there is an item at starting position of drag operation and the item
   * is not selected, select it.
   * @param e The event object for 'dragstart'.
   */
  private handleDragStart_(e: DragEvent) {
    const target = e.target as HTMLElement;
    const element = target.ownerDocument.elementFromPoint(
                        e.clientX, e.clientY) as HTMLElement;
    const listItem = this.getListItemAncestor(element);
    if (!listItem) {
      return;
    }

    const index = this.getIndexOfListItem(listItem);
    if (index === -1) {
      return;
    }

    const isAlreadySelected = this.selectionModel_!.getIndexSelected(index);
    if (!isAlreadySelected) {
      this.selectionModel_!.selectedIndex = index;
    }
  }

  get disabled(): boolean {
    return this.hasAttribute('disabled');
  }

  set disabled(value: boolean) {
    boolAttrSetter(this, 'disabled', value);
  }

  /**
   * Whether the list or one of its descendents has focus. This is necessary
   * because list items can contain controls that can be focused, and for some
   * purposes (e.g., styling), the list can still be conceptually focused at
   * that point even though it doesn't actually have the page focus.
   */
  get hasElementFocus(): boolean {
    return this.hasAttribute('hasElementFocus');
  }

  set hasElementFocus(value: boolean) {
    boolAttrSetter(this, 'hasElementFocus', value);
  }

  /**
   * Obtains the index list of elements that are hit by a point or rectangle.
   *
   * @param x X coordinate value.
   * @param y Y coordinate value.
   * @param width Width of the coordinate.
   * @param height Height of the coordinate.
   * @return Indexes of the hit elements.
   */
  abstract getHitElements(
      x: number, y: number, width?: number, height?: number): number[];
}


/**
 * Check if |start| or its ancestor under |root| is focusable.
 * This is a helper for handleMouseDown.
 * @param start An element which we start to check.
 * @param root An element which we finish to check.
 * @return True if we found a focusable element.
 */
function containsFocusableElement(start: HTMLElement, root: ListItem): boolean {
  for (let element: HTMLElement|null = start; element && element !== root;
       element = element.parentElement) {
    if (element.tabIndex >= 0 && isDisabled(element)) {
      return true;
    }
  }
  return false;
}

function isDisabled(element: HTMLElement) {
  if ('disabled' in element && element.disabled) {
    return true;
  }
  return false;
}

export type CachedItemRestored = CustomEvent<ListItem>;

declare global {
  interface HTMLElementEventMap {
    'cachedItemRestored': CachedItemRestored;
  }
}
