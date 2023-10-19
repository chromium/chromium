// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getPropertyDescriptor, PropertyKind, dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';

import {ArrayDataModel} from '../../../common/js/array_data_model.js';
import {define as crUiDefine} from '../../../common/js/ui.js';

import {ListSelectionModel} from './list_selection_model.js';
import {ListSelectionController} from './list_selection_controller.js';
import {ListItem, createListItem} from './list_item.js';

/**
 * @fileoverview This implements a list control.
 */


/**
 *  @typedef {{
 *    height: number,
 *    marginBottom: number,
 *    marginLeft: number,
 *    marginRight: number,
 *    marginTop: number,
 *    width: number
 *  }}
 */
// @ts-ignore: error TS7005: Variable 'Size' implicitly has an 'any' type.
export let Size;

/**
 * Whether a mouse event is inside the element viewport. This will return
 * false if the mouseevent was generated over a border or a scrollbar.
 * @param {!HTMLElement} el The element to test the event with.
 * @param {!Event} e The mouse event.
 * @return {boolean} Whether the mouse event was inside the viewport.
 */
function inViewport(el, e) {
  const rect = el.getBoundingClientRect();
  // @ts-ignore: error TS2339: Property 'clientX' does not exist on type
  // 'Event'.
  const x = e.clientX;
  // @ts-ignore: error TS2339: Property 'clientY' does not exist on type
  // 'Event'.
  const y = e.clientY;
  return x >= rect.left + el.clientLeft &&
      x < rect.left + el.clientLeft + el.clientWidth &&
      y >= rect.top + el.clientTop &&
      y < rect.top + el.clientTop + el.clientHeight;
}

// @ts-ignore: error TS7006: Parameter 'el' implicitly has an 'any' type.
function getComputedStyle(el) {
  return el.ownerDocument.defaultView.getComputedStyle(el);
}

/**
 * Creates a new list element.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLUListElement}
 */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const List = crUiDefine('list');

List.prototype = {
  __proto__: HTMLUListElement.prototype,

  /**
   * Measured size of list items. This is lazily calculated the first time it
   * is needed. Note that lead item is allowed to have a different height, to
   * accommodate lists where a single item at a time can be expanded to show
   * more detail.
   * @type {?Size}
   * @private
   */
  measured_: null,

  /**
   * Whether or not the list is autoexpanding. If true, the list resizes
   * its height to accommodate all children.
   * @type {boolean}
   * @private
   */
  autoExpands_: false,

  /**
   * Whether or not the rows on list have various heights. If true, all the
   * rows have the same fixed height. Otherwise, each row resizes its height
   * to accommodate all contents.
   * @type {boolean}
   * @private
   */
  fixedHeight_: true,

  /**
   * Whether or not the list view has a blank space below the last row.
   * @type {boolean}
   * @private
   */
  remainingSpace_: true,

  /**
   * Function used to create grid items.
   * @type {function(*): ListItem}
   * @protected
   */
  itemConstructor_: createListItem,

  /**
   * Function used to create grid items.
   * @return {function(*): ListItem}
   */
  get itemConstructor() {
    return this.itemConstructor_;
  },
  set itemConstructor(func) {
    if (func !== this.itemConstructor_) {
      this.itemConstructor_ = func;
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItems_ = {};
      this.redraw();
    }
  },

  dataModel_: null,

  /**
   * The data model driving the list.
   * @type {ArrayDataModel}
   */
  set dataModel(dataModel) {
    if (this.dataModel_ === dataModel) {
      return;
    }

    // @ts-ignore: error TS2551: Property 'boundHandleDataModelPermuted_' does
    // not exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'handleDataModelPermuted_'?
    if (!this.boundHandleDataModelPermuted_) {
      // @ts-ignore: error TS2551: Property 'boundHandleDataModelPermuted_' does
      // not exist on type '{ __proto__: HTMLUListElement; measured_: Size |
      // null; autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
      // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleDataModelPermuted_'?
      this.boundHandleDataModelPermuted_ =
          this.handleDataModelPermuted_.bind(this);
      // @ts-ignore: error TS2551: Property 'boundHandleDataModelChange_' does
      // not exist on type '{ __proto__: HTMLUListElement; measured_: Size |
      // null; autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
      // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleDataModelChange_'?
      this.boundHandleDataModelChange_ = this.handleDataModelChange_.bind(this);
    }

    if (this.dataModel_) {
      // @ts-ignore: error TS2339: Property 'removeEventListener' does not exist
      // on type 'never'.
      this.dataModel_.removeEventListener(
          // @ts-ignore: error TS2551: Property 'boundHandleDataModelPermuted_'
          // does not exist on type '{ __proto__: HTMLUListElement; measured_:
          // Size | null; autoExpands_: boolean; fixedHeight_: boolean;
          // remainingSpace_: boolean; itemConstructor_: (arg0: any) =>
          // ListItem; ... 56 more ...; startDragSelection(event: Event): void;
          // }'. Did you mean 'handleDataModelPermuted_'?
          'permuted', this.boundHandleDataModelPermuted_);
      // @ts-ignore: error TS2339: Property 'removeEventListener' does not exist
      // on type 'never'.
      this.dataModel_.removeEventListener(
          // @ts-ignore: error TS2551: Property 'boundHandleDataModelChange_'
          // does not exist on type '{ __proto__: HTMLUListElement; measured_:
          // Size | null; autoExpands_: boolean; fixedHeight_: boolean;
          // remainingSpace_: boolean; itemConstructor_: (arg0: any) =>
          // ListItem; ... 56 more ...; startDragSelection(event: Event): void;
          // }'. Did you mean 'handleDataModelChange_'?
          'change', this.boundHandleDataModelChange_);
    }

    this.dataModel_ = dataModel;

    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItems_ = {};
    // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItemHeights_ = {};
    this.selectionModel.clear();
    if (dataModel) {
      // @ts-ignore: error TS2339: Property 'length' does not exist on type
      // 'never'.
      this.selectionModel.adjustLength(dataModel.length);
    }

    if (this.dataModel_) {
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type 'never'.
      this.dataModel_.addEventListener(
          // @ts-ignore: error TS2551: Property 'boundHandleDataModelPermuted_'
          // does not exist on type '{ __proto__: HTMLUListElement; measured_:
          // Size | null; autoExpands_: boolean; fixedHeight_: boolean;
          // remainingSpace_: boolean; itemConstructor_: (arg0: any) =>
          // ListItem; ... 56 more ...; startDragSelection(event: Event): void;
          // }'. Did you mean 'handleDataModelPermuted_'?
          'permuted', this.boundHandleDataModelPermuted_);
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type 'never'.
      this.dataModel_.addEventListener(
          // @ts-ignore: error TS2551: Property 'boundHandleDataModelChange_'
          // does not exist on type '{ __proto__: HTMLUListElement; measured_:
          // Size | null; autoExpands_: boolean; fixedHeight_: boolean;
          // remainingSpace_: boolean; itemConstructor_: (arg0: any) =>
          // ListItem; ... 56 more ...; startDragSelection(event: Event): void;
          // }'. Did you mean 'handleDataModelChange_'?
          'change', this.boundHandleDataModelChange_);
    }

    this.redraw();
    this.onSetDataModelComplete();
  },

  get dataModel() {
    return this.dataModel_;
  },

  /**
   * Override to be notified when |this.dataModel| is set.
   * @protected
   */
  onSetDataModelComplete() {},

  /**
   * Cached item for measuring the default item size by measureItem().
   * @type {ListItem}
   */
  // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'ListItem'.
  cachedMeasuredItem_: null,

  /**
   * The selection model to use.
   * @type {ListSelectionModel}
   */
  get selectionModel() {
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'selectionModel'?
    return this.selectionModel_;
  },
  set selectionModel(sm) {
    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'selectionModel'?
    const oldSm = this.selectionModel_;
    if (oldSm === sm) {
      return;
    }

    // @ts-ignore: error TS2551: Property 'boundHandleOnChange_' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'handleOnChange_'?
    if (!this.boundHandleOnChange_) {
      // @ts-ignore: error TS2551: Property 'boundHandleOnChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleOnChange_'?
      this.boundHandleOnChange_ = this.handleOnChange_.bind(this);
      // @ts-ignore: error TS2551: Property 'boundHandleLeadChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleLeadChange'?
      this.boundHandleLeadChange_ = this.handleLeadChange.bind(this);
    }

    if (oldSm) {
      // @ts-ignore: error TS2551: Property 'boundHandleOnChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleOnChange_'?
      oldSm.removeEventListener('change', this.boundHandleOnChange_);
      // @ts-ignore: error TS2551: Property 'boundHandleLeadChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleLeadChange'?
      oldSm.removeEventListener('leadIndexChange', this.boundHandleLeadChange_);
    }

    // @ts-ignore: error TS2551: Property 'selectionModel_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'selectionModel'?
    this.selectionModel_ = sm;
    // @ts-ignore: error TS2551: Property 'selectionController_' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'createSelectionController'?
    this.selectionController_ = this.createSelectionController(sm);

    if (sm) {
      // @ts-ignore: error TS2551: Property 'boundHandleOnChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleOnChange_'?
      sm.addEventListener('change', this.boundHandleOnChange_);
      // @ts-ignore: error TS2551: Property 'boundHandleLeadChange_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleLeadChange'?
      sm.addEventListener('leadIndexChange', this.boundHandleLeadChange_);
    }
  },

  /**
   * Whether or not the list auto-expands.
   * @type {boolean}
   */
  get autoExpands() {
    return this.autoExpands_;
  },
  set autoExpands(autoExpands) {
    if (this.autoExpands_ === autoExpands) {
      return;
    }
    this.autoExpands_ = autoExpands;
    this.redraw();
  },

  /**
   * Whether or not the rows on list have various heights.
   * @type {boolean}
   */
  get fixedHeight() {
    return this.fixedHeight_;
  },
  set fixedHeight(fixedHeight) {
    if (this.fixedHeight_ === fixedHeight) {
      return;
    }
    this.fixedHeight_ = fixedHeight;
    this.redraw();
  },

  /**
   * Convenience alias for selectionModel.selectedItem
   * @type {*}
   */
  get selectedItem() {
    const dataModel = this.dataModel;
    if (dataModel) {
      const index = this.selectionModel.selectedIndex;
      if (index !== -1) {
        // @ts-ignore: error TS2339: Property 'item' does not exist on type
        // 'never'.
        return dataModel.item(index);
      }
    }
    return null;
  },
  set selectedItem(selectedItem) {
    const dataModel = this.dataModel;
    if (dataModel) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const index = this.dataModel.indexOf(selectedItem);
      this.selectionModel.selectedIndex = index;
    }
  },

  /**
   * Convenience alias for selectionModel.selectedItems
   * @type {!Array<*>}
   */
  get selectedItems() {
    const indexes = this.selectionModel.selectedIndexes;
    const dataModel = this.dataModel;
    if (dataModel) {
      return indexes.map(function(i) {
        // @ts-ignore: error TS2339: Property 'item' does not exist on type
        // 'never'.
        return dataModel.item(i);
      });
    }
    return [];
  },

  /**
   * The HTML elements representing the items.
   * @type {HTMLCollection}
   */
  get items() {
    // @ts-ignore: error TS2339: Property 'children' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    return Array.prototype.filter.call(this.children, this.isItem, this);
  },

  /**
   * Returns true if the child is a list item. Subclasses may override this
   * to filter out certain elements.
   * @param {Node} child Child of the list.
   * @return {boolean} True if a list item.
   */
  isItem(child) {
    return child.nodeType === Node.ELEMENT_NODE &&
        // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        child !== this.beforeFiller_ && child !== this.afterFiller_;
  },

  batchCount_: 0,

  /**
   * When making a lot of updates to the list, the code could be wrapped in
   * the startBatchUpdates and finishBatchUpdates to increase performance. Be
   * sure that the code will not return without calling endBatchUpdates or the
   * list will not be correctly updated.
   */
  startBatchUpdates() {
    this.batchCount_++;
  },

  /**
   * See startBatchUpdates.
   */
  endBatchUpdates() {
    this.batchCount_--;
    if (this.batchCount_ === 0) {
      this.redraw();
    }
  },

  /**
   * Initializes the element.
   */
  decorate() {
    // Add fillers.
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.beforeFiller_ = this.ownerDocument.createElement('div');
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.afterFiller_ = this.ownerDocument.createElement('div');
    // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.beforeFiller_.className = 'spacer';
    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.afterFiller_.className = 'spacer';
    // @ts-ignore: error TS2339: Property 'textContent' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.textContent = '';
    // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.appendChild(this.beforeFiller_);
    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.appendChild(this.afterFiller_);

    // @ts-ignore: error TS2339: Property 'length' does not exist on type
    // 'never'.
    const length = this.dataModel ? this.dataModel.length : 0;
    this.selectionModel = new ListSelectionModel(length);

    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('dblclick', this.handleDoubleClick_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('mousedown', handleMouseDown);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('dragstart', handleDragStart, true);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('mouseup', this.handlePointerDownUp_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('keydown', this.handleKeyDown);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('focus', this.handleElementFocus_, true);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('blur', this.handleElementBlur_, true);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('scroll', this.handleScroll.bind(this));
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('touchstart', this.handleTouchEvents_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('touchmove', this.handleTouchEvents_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('touchend', this.handleTouchEvents_);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.addEventListener('touchcancel', this.handleTouchEvents_);
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.setAttribute('role', 'list');

    // Make list focusable
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (!this.hasAttribute('tabindex')) {
      // @ts-ignore: error TS2339: Property 'tabIndex' does not exist on type '{
      // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.tabIndex = 0;
    }
  },

  /**
   * @param {ListItem=} item The list item to measure.
   * @return {number} The height of the given item. If the fixed height on CSS
   * is set by 'px', uses that value as height. Otherwise, measures the size.
   * @private
   */
  measureItemHeight_(item) {
    return this.measureItem(item).height;
  },

  /**
   * @return {number} The height of default item, measuring it if necessary.
   * @protected
   */
  getDefaultItemHeight_() {
    return this.getDefaultItemSize_().height;
  },

  /**
   * @param {number} index The index of the item.
   * @return {number} The height of the item, measuring it if necessary.
   */
  getItemHeightByIndex_(index) {
    // If |this.fixedHeight_| is true, all the rows have same default height.
    if (this.fixedHeight_) {
      return this.getDefaultItemHeight_();
    }

    // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.cachedItemHeights_[index]) {
      // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist
      // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      return this.cachedItemHeights_[index];
    }

    const item = this.getListItemByIndex(index);
    if (item) {
      const h = this.measureItemHeight_(item);
      // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist
      // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItemHeights_[index] = h;
      return h;
    }
    return this.getDefaultItemHeight_();
  },

  /**
   * @return {!Size} The height and width of default item, measuring it
   *     if necessary.
   * @protected
   */
  getDefaultItemSize_() {
    if (!this.measured_ || !this.measured_.height) {
      this.measured_ = this.measureItem();
    }
    return this.measured_;
  },

  /**
   * Creates an item (dataModel.item(0)) and measures its height. The item is
   * cached instead of creating a new one every time..
   * @param {ListItem=} opt_item The list item to use to do the
   *     measuring. If this is not provided an item will be created based on
   *     the first value in the model.
   * @return {!Size} The height and width of the item, taking margins
   *     into account, and the top, bottom, left and right margins themselves.
   */
  measureItem(opt_item) {
    const dataModel = this.dataModel;
    // @ts-ignore: error TS2339: Property 'length' does not exist on type
    // 'never'.
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
    const item = opt_item || this.cachedMeasuredItem_ ||
        // @ts-ignore: error TS2339: Property 'item' does not exist on type
        // 'never'.
        this.createItem(dataModel.item(0));
    if (!opt_item) {
      this.cachedMeasuredItem_ = item;
      // @ts-ignore: error TS2339: Property 'appendChild' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.appendChild(item);
    }

    const rect = item.getBoundingClientRect();
    const cs = getComputedStyle(item);
    const mt = parseFloat(cs.marginTop);
    const mb = parseFloat(cs.marginBottom);
    const ml = parseFloat(cs.marginLeft);
    const mr = parseFloat(cs.marginRight);
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

    if (!opt_item) {
      // @ts-ignore: error TS2339: Property 'removeChild' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.removeChild(item);
    }
    return {
      height: Math.max(0, h),
      marginTop: mt,
      marginBottom: mb,
      width: Math.max(0, w),
      marginLeft: ml,
      marginRight: mr,
    };
  },

  /**
   * Callback for the double click event.
   * @param {Event} e The mouse event object.
   * @private
   */
  handleDoubleClick_(e) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.disabled) {
      return;
    }

    const target = /** @type {HTMLElement} */ (e.target);

    const ancestor = this.getListItemAncestor(target);
    let index = -1;
    if (ancestor) {
      index = this.getIndexOfListItem(ancestor);
      this.activateItemAtIndex(index);
    }

    const sm = this.selectionModel;
    const indexSelected = sm.getIndexSelected(index);
    if (!indexSelected) {
      this.handlePointerDownUp_(e);
    }
  },

  /**
   * Callback for mousedown and mouseup events.
   * @param {Event} e The mouse event object.
   * @private
   */
  handlePointerDownUp_(e) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.disabled) {
      return;
    }

    let target = /** @type {HTMLElement} */ (e.target);

    // If the target was this element we need to make sure that the user did
    // not click on a border or a scrollbar.
    // @ts-ignore: error TS2367: This comparison appears to be unintentional
    // because the types 'HTMLElement' and '{ __proto__: HTMLUListElement;
    // measured_: Size | null; autoExpands_: boolean; fixedHeight_: boolean;
    // remainingSpace_: boolean; itemConstructor_: (arg0: any) => ListItem; ...
    // 56 more ...; startDragSelection(event: Event): void; }' have no overlap.
    if (target === this) {
      if (inViewport(target, e)) {
        // @ts-ignore: error TS2339: Property 'selectionController_' does not
        // exist on type 'never'.
        this.selectionController_.handlePointerDownUp(e, -1);
      }
      return;
    }

    target = this.getListItemAncestor(target);
    if (!target) {
      return;
    }

    // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
    // assignable to parameter of type 'HTMLLIElement'.
    const index = this.getIndexOfListItem(target);
    // @ts-ignore: error TS2551: Property 'selectionController_' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'createSelectionController'?
    this.selectionController_.handlePointerDownUp(e, index);
  },

  /**
   * Called when an element in the list is focused. Marks the list as having
   * a focused element, and dispatches an event if it didn't have focus.
   * @param {Event} e The focus event.
   * @private
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleElementFocus_(e) {
    // @ts-ignore: error TS2551: Property 'hasElementFocus' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'handleElementFocus_'?
    if (!this.hasElementFocus) {
      // @ts-ignore: error TS2551: Property 'hasElementFocus' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleElementFocus_'?
      this.hasElementFocus = true;
    }
  },

  /**
   * Called when an element in the list is blurred. If focus moves outside
   * the list, marks the list as no longer having focus and dispatches an
   * event.
   * @param {Event} e The blur event.
   * @private
   * @suppress {checkTypes}
   * TODO(dbeam): remove suppression when the extern
   * Node.prototype.contains() will be fixed.
   */
  handleElementBlur_(e) {
    // @ts-ignore: error TS2339: Property 'relatedTarget' does not exist on type
    // 'Event'.
    if (!this.contains(e.relatedTarget)) {
      // @ts-ignore: error TS2551: Property 'hasElementFocus' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'handleElementFocus_'?
      this.hasElementFocus = false;
    }
  },

  /**
   * Returns the list item element containing the given element, or null if
   * it doesn't belong to any list item element.
   * @param {HTMLElement} element The element.
   * @return {HTMLLIElement} The list item containing |element|, or null.
   */
  getListItemAncestor(element) {
    let container = element;
    // @ts-ignore: error TS2367: This comparison appears to be unintentional
    // because the types 'ParentNode | null' and '{ __proto__: HTMLUListElement;
    // measured_: Size | null; autoExpands_: boolean; fixedHeight_: boolean;
    // remainingSpace_: boolean; itemConstructor_: (arg0: any) => ListItem; ...
    // 56 more ...; startDragSelection(event: Event): void; }' have no overlap.
    while (container && container.parentNode !== this) {
      // @ts-ignore: error TS2322: Type 'ParentNode | null' is not assignable to
      // type 'HTMLElement'.
      container = container.parentNode;
    }

    // @ts-ignore: error TS2322: Type 'HTMLLIElement | null' is not assignable
    // to type 'HTMLLIElement'.
    return (container instanceof HTMLLIElement ? container : null);
  },

  /**
   * Handle a keydown event.
   * @param {Event} e The keydown event.
   */
  handleKeyDown(e) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (!this.disabled) {
      // @ts-ignore: error TS2551: Property 'selectionController_' does not
      // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'. Did you mean
      // 'createSelectionController'?
      this.selectionController_.handleKeyDown(e);
    }
  },

  /**
   * Handle a scroll event.
   * @param {Event} e The scroll event.
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleScroll(e) {
    requestAnimationFrame(this.redraw.bind(this));
  },

  /**
   * Handle touchmove/touchcancel events.
   * @param {!Event} e The event.
   * @private
   */
  handleTouchEvents_(e) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.disabled) {
      return;
    }

    let target = /** @type {HTMLElement} */ (e.target);

    // @ts-ignore: error TS2367: This comparison appears to be unintentional
    // because the types 'HTMLElement' and '{ __proto__: HTMLUListElement;
    // measured_: Size | null; autoExpands_: boolean; fixedHeight_: boolean;
    // remainingSpace_: boolean; itemConstructor_: (arg0: any) => ListItem; ...
    // 56 more ...; startDragSelection(event: Event): void; }' have no overlap.
    if (target === this) {
      // Unlike the mouse events, we don't check if the touch is inside the
      // viewport because of these reasons:
      // - The scrollbars do not interact with touch.
      // - touch* events are not sent to this element when tapping or
      //   dragging window borders by touch.
      // @ts-ignore: error TS2339: Property 'selectionController_' does not
      // exist on type 'never'.
      this.selectionController_.handleTouchEvents(e, -1);
      return;
    }

    target = this.getListItemAncestor(target);
    if (!target) {
      return;
    }

    // @ts-ignore: error TS2345: Argument of type 'HTMLElement' is not
    // assignable to parameter of type 'HTMLLIElement'.
    const index = this.getIndexOfListItem(target);
    // @ts-ignore: error TS2551: Property 'selectionController_' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'. Did you mean
    // 'createSelectionController'?
    this.selectionController_.handleTouchEvents(e, index);
  },

  /**
   * Callback from the selection model. We dispatch {@code change} events
   * when the selection changes.
   * @param {!Event} ce Event with change info.
   * @private
   */
  handleOnChange_(ce) {
    // @ts-ignore: error TS7006: Parameter 'change' implicitly has an 'any'
    // type.
    ce.changes.forEach(function(change) {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const listItem = this.getListItemByIndex(change.index);
      if (listItem) {
        listItem.selected = change.selected;
        listItem.setAttribute('aria-selected', listItem.selected);
        if (change.selected) {
          listItem.setAttribute('aria-posinset', change.index + 1);
          // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because
          // it does not have a type annotation.
          listItem.setAttribute('aria-setsize', this.dataModel.length);
        } else {
          listItem.removeAttribute('aria-posinset');
          listItem.removeAttribute('aria-setsize');
        }
      }
    }, this);

    // @ts-ignore: error TS2345: Argument of type '{ __proto__:
    // HTMLUListElement; measured_: Size | null; autoExpands_: boolean;
    // fixedHeight_: boolean; remainingSpace_: boolean; itemConstructor_: (arg0:
    // any) => ListItem; ... 56 more ...; startDragSelection(event: Event):
    // void; }' is not assignable to parameter of type 'EventTarget'.
    dispatchSimpleEvent(this, 'change');
  },

  /**
   * Handles a change of the lead item from the selection model.
   * @param {Event} e The property change event.
   * @protected
   */
  handleLeadChange(e) {
    let element;
    // @ts-ignore: error TS2339: Property 'oldValue' does not exist on type
    // 'Event'.
    if (e.oldValue !== -1) {
      // @ts-ignore: error TS2339: Property 'oldValue' does not exist on type
      // 'Event'.
      if ((element = this.getListItemByIndex(e.oldValue))) {
        element.lead = false;
      }
    }

    // @ts-ignore: error TS2339: Property 'newValue' does not exist on type
    // 'Event'.
    if (e.newValue !== -1) {
      // @ts-ignore: error TS2339: Property 'newValue' does not exist on type
      // 'Event'.
      if ((element = this.getListItemByIndex(e.newValue))) {
        element.lead = true;
      }
      // @ts-ignore: error TS2339: Property 'newValue' does not exist on type
      // 'Event'.
      if (e.oldValue !== e.newValue) {
        if (element) {
          // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
          // type '{ __proto__: HTMLUListElement; measured_: Size | null;
          // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
          // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more
          // ...; startDragSelection(event: Event): void; }'.
          this.setAttribute('aria-activedescendant', element.id);
        }
        // @ts-ignore: error TS2339: Property 'newValue' does not exist on type
        // 'Event'.
        this.scrollIndexIntoView(e.newValue);
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
        const self = this;
        window.setTimeout(function() {
          // @ts-ignore: error TS2339: Property 'newValue' does not exist on
          // type 'Event'.
          self.scrollIndexIntoView(e.newValue);
        }, 0);
      }
    } else {
      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.removeAttribute('aria-activedescendant');
    }
  },

  /**
   * This handles data model 'permuted' event.
   * this event is dispatched as a part of sort or splice.
   * We need to
   *  - adjust the cache.
   *  - adjust selection.
   *  - redraw. (called in this.endBatchUpdates())
   *  It is important that the cache adjustment happens before selection model
   *  adjustments.
   * @param {Event} e The 'permuted' event.
   */
  handleDataModelPermuted_(e) {
    const newCachedItems = {};
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    for (const index in this.cachedItems_) {
      // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
      // 'Event'.
      if (e.permutation[index] !== -1) {
        // @ts-ignore: error TS2339: Property 'permutation' does not exist on
        // type 'Event'.
        const newIndex = e.permutation[index];
        // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        newCachedItems[newIndex] = this.cachedItems_[index];
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'any' can't be used to index type '{}'.
        newCachedItems[newIndex].listIndex = newIndex;
      }
    }
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItems_ = newCachedItems;
    // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.pinnedItem_ = null;

    const newCachedItemHeights = {};
    // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    for (const index in this.cachedItemHeights_) {
      // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
      // 'Event'.
      if (e.permutation[index] !== -1) {
        // @ts-ignore: error TS2339: Property 'permutation' does not exist on
        // type 'Event'.
        newCachedItemHeights[e.permutation[index]] =
            // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not
            // exist on type '{ __proto__: HTMLUListElement; measured_: Size |
            // null; autoExpands_: boolean; fixedHeight_: boolean;
            // remainingSpace_: boolean; itemConstructor_: (arg0: any) =>
            // ListItem; ... 56 more ...; startDragSelection(event: Event):
            // void; }'.
            this.cachedItemHeights_[index];
      }
    }
    // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItemHeights_ = newCachedItemHeights;

    this.startBatchUpdates();

    const sm = this.selectionModel;
    // @ts-ignore: error TS2339: Property 'newLength' does not exist on type
    // 'Event'.
    sm.adjustLength(e.newLength);
    // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
    // 'Event'.
    sm.adjustToReordering(e.permutation);

    this.endBatchUpdates();
  },

  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleDataModelChange_(e) {
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    delete this.cachedItems_[e.index];
    // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    delete this.cachedItemHeights_[e.index];
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'ListItem'.
    this.cachedMeasuredItem_ = null;

    // @ts-ignore: error TS2339: Property 'firstIndex_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (e.index >= this.firstIndex_ &&
        // @ts-ignore: error TS2339: Property 'lastIndex_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        (e.index < this.lastIndex_ || this.remainingSpace_)) {
      this.redraw();
    }
  },

  /**
   * @param {number} index The index of the item.
   * @return {number} The top position of the item inside the list.
   */
  getItemTop(index) {
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
  },

  /**
   * @param {number} index The index of the item.
   * @return {number} The row of the item. May vary in the case
   *     of multiple columns.
   */
  getItemRow(index) {
    return index;
  },

  /**
   * @param {number} row The row.
   * @return {number} The index of the first item in the row.
   */
  getFirstItemInRow(row) {
    return row;
  },

  /**
   * Ensures that a given index is inside the viewport.
   * @param {number} index The index of the item to scroll into view.
   */
  scrollIndexIntoView(index) {
    const dataModel = this.dataModel;
    // @ts-ignore: error TS2339: Property 'length' does not exist on type
    // 'never'.
    if (!dataModel || index < 0 || index >= dataModel.length) {
      return;
    }

    const itemHeight = this.getItemHeightByIndex_(index);
    // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const scrollTop = this.scrollTop;
    const top = this.getItemTop(index);
    // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const clientHeight = this.clientHeight;

    const cs = getComputedStyle(this);
    const paddingY =
        parseInt(cs.paddingTop, 10) + parseInt(cs.paddingBottom, 10);
    const availableHeight = clientHeight - paddingY;

    const self = this;
    // Function to adjust the tops of viewport and row.
    function scrollToAdjustTop() {
      // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      self.scrollTop = top;
    }
    // Function to adjust the bottoms of viewport and row.
    function scrollToAdjustBottom() {
      // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
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
  },

  /**
   * @return {!ClientRect} The rect to use for the context menu.
   */
  getRectForContextMenu() {
    // TODO(arv): Add trait support so we can share more code between trees
    // and lists.
    const index = this.selectionModel.selectedIndex;
    const el = this.getListItemByIndex(index);
    if (el) {
      return el.getBoundingClientRect();
    }
    // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    return this.getBoundingClientRect();
  },

  /**
   * Takes a value from the data model and finds the associated list item.
   * @param {*} value The value in the data model that we want to get the list
   *     item for.
   * @return {ListItem} The first found list item or null if not found.
   */
  getListItem(value) {
    const dataModel = this.dataModel;
    if (dataModel) {
      // @ts-ignore: error TS2339: Property 'indexOf' does not exist on type
      // 'never'.
      const index = dataModel.indexOf(value);
      return this.getListItemByIndex(index);
    }
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'ListItem'.
    return null;
  },

  /**
   * Find the list item element at the given index.
   * @param {number} index The index of the list item to get.
   * @return {ListItem} The found list item or null if not found.
   */
  getListItemByIndex(index) {
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    return this.cachedItems_[index] || null;
  },

  /**
   * Find the index of the given list item element.
   * @param {HTMLLIElement} item The list item to get the index of.
   * @return {number} The index of the list item, or -1 if not found.
   */
  getIndexOfListItem(item) {
    // @ts-ignore: error TS2339: Property 'listIndex' does not exist on type
    // 'HTMLLIElement'.
    const index = item.listIndex;
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.cachedItems_[index] === item) {
      return index;
    }
    return -1;
  },

  /**
   * Creates a new list item.
   * @param {?} value The value to use for the item.
   * @return {!ListItem} The newly created list item.
   */
  createItem(value) {
    const item = this.itemConstructor_(value);
    item.label = value;
    if (typeof item.decorate === 'function') {
      item.decorate();
    }
    return item;
  },

  /**
   * Creates the selection controller to use internally.
   * @param {ListSelectionModel} sm The underlying selection model.
   * @return {!ListSelectionController} The newly created selection
   *     controller.
   */
  createSelectionController(sm) {
    return new ListSelectionController(sm);
  },

  /**
   * Return the heights (in pixels) of the top of the given item index within
   * the list, and the height of the given item itself, accounting for the
   * possibility that the lead item may be a different height.
   * @param {number} index The index to find the top height of.
   * @return {{top: number, height: number}} The heights for the given index.
   */
  getHeightsForIndex(index) {
    const itemHeight = this.getItemHeightByIndex_(index);
    const top = this.getItemTop(index);
    return {top: top, height: itemHeight};
  },

  /**
   * Find the index of the list item containing the given y offset (measured
   * in pixels from the top) within the list. In the case of multiple columns,
   * returns the first index in the row.
   * @param {number} offset The y offset in pixels to get the index of.
   * @return {number} The index of the list item. Returns the list size if
   *     given offset exceeds the height of list.
   * @protected
   */
  getIndexForListOffset_(offset) {
    const itemHeight = this.getDefaultItemHeight_();
    if (!itemHeight) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      return this.dataModel.length;
    }

    if (this.fixedHeight_) {
      return this.getFirstItemInRow(Math.floor(offset / itemHeight));
    }

    // If offset exceeds the height of list.
    let lastHeight = 0;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    if (this.dataModel.length) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const h = this.getHeightsForIndex(this.dataModel.length - 1);
      lastHeight = h.top + h.height;
    }
    if (lastHeight < offset) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      return this.dataModel.length;
    }

    // Estimates index.
    let estimatedIndex =
        // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      // @ts-ignore: error TS2531: Object is possibly 'null'.
    } while (0 < estimatedIndex && estimatedIndex < this.dataModel.length);

    return estimatedIndex;
  },

  /**
   * Return the number of items that occupy the range of heights between the
   * top of the start item and the end offset.
   * @param {number} startIndex The index of the first visible item.
   * @param {number} endOffset The y offset in pixels of the end of the list.
   * @return {number} The number of list items visible.
   * @protected
   */
  countItemsInRange_(startIndex, endOffset) {
    const endIndex = this.getIndexForListOffset_(endOffset);
    return endIndex - startIndex + 1;
  },

  /**
   * Calculates the number of items fitting in the given viewport.
   * @param {number} scrollTop The scroll top position.
   * @param {number} clientHeight The height of viewport.
   * @return {{first: number, length: number, last: number}} The index of
   *     first item in view port, The number of items, The item past the last.
   */
  getItemsInViewPort(scrollTop, clientHeight) {
    if (this.autoExpands_) {
      return {
        first: 0,
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        length: this.dataModel.length,
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        last: this.dataModel.length,
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
  },

  /**
   * Merges list items currently existing in the list with items in the range
   * [firstIndex, lastIndex). Removes or adds items if needed.
   * Doesn't delete {@code this.pinnedItem_} if it is present (instead hides
   * it if it is out of the range).
   * @param {number} firstIndex The index of first item, inclusively.
   * @param {number} lastIndex The index of last item, exclusively.
   */
  mergeItems(firstIndex, lastIndex) {
    const self = this;
    const dataModel = this.dataModel;
    let currentIndex = firstIndex;

    function insert() {
      // @ts-ignore: error TS18047: 'dataModel' is possibly 'null'.
      const dataItem = dataModel.item(currentIndex);
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      const cachedCurrentItem = self.cachedItems_[currentIndex];
      if (cachedCurrentItem) {
        // Emit synthetic event with cached item that is about to be restored.
        // @ts-ignore: error TS2339: Property 'dispatchEvent' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        self.dispatchEvent(new CustomEvent('cachedItemRestored', {
          detail: cachedCurrentItem,
        }));
      }
      const newItem = cachedCurrentItem || self.createItem(dataItem);
      newItem.listIndex = currentIndex;
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      self.cachedItems_[currentIndex] = newItem;
      // @ts-ignore: error TS7005: Variable 'item' implicitly has an 'any' type.
      self.insertBefore(newItem, item);
      currentIndex++;
    }

    function remove() {
      // @ts-ignore: error TS7005: Variable 'item' implicitly has an 'any' type.
      const next = item.nextSibling;
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      if (item !== self.pinnedItem_) {
        // @ts-ignore: error TS7005: Variable 'item' implicitly has an 'any'
        // type.
        self.removeChild(item);
      }
      item = next;
    }

    // @ts-ignore: error TS7034: Variable 'item' implicitly has type 'any' in
    // some locations where its type cannot be determined.
    let item;
    // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    for (item = this.beforeFiller_.nextSibling;
         // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on
         // type '{ __proto__: HTMLUListElement; measured_: Size | null;
         // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
         // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
         // startDragSelection(event: Event): void; }'.
         item !== this.afterFiller_ && currentIndex < lastIndex;) {
      if (!this.isItem(item)) {
        item = item.nextSibling;
        continue;
      }

      const index = item.listIndex;
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      if (this.cachedItems_[index] !== item || index < currentIndex) {
        remove();
      } else if (index === currentIndex) {
        // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.cachedItems_[currentIndex] = item;
        item = item.nextSibling;
        currentIndex++;
      } else {  // index > currentIndex
        insert();
      }
    }

    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    while (item !== this.afterFiller_) {
      if (this.isItem(item)) {
        remove();
      } else {
        item = item.nextSibling;
      }
    }

    // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.pinnedItem_) {
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      const index = this.pinnedItem_.listIndex;
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.pinnedItem_.hidden = index < firstIndex || index >= lastIndex;
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItems_[index] = this.pinnedItem_;
      if (index >= lastIndex) {
        // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        item = this.pinnedItem_;
      }  // Insert new items before this one.
    }

    while (currentIndex < lastIndex) {
      insert();
    }
  },

  /**
   * Ensures that all the item sizes in the list have been already cached.
   */
  ensureAllItemSizesInCache() {
    const measuringIndexes = [];
    const isElementAppended = [];
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    for (let y = 0; y < this.dataModel.length; y++) {
      // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist
      // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      if (!this.cachedItemHeights_[y]) {
        measuringIndexes.push(y);
        isElementAppended.push(false);
      }
    }

    const measuringItems = [];
    // Adds temporary elements.
    for (let y = 0; y < measuringIndexes.length; y++) {
      const index = measuringIndexes[y];
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const dataItem = this.dataModel.item(index);
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      const listItem = this.cachedItems_[index] || this.createItem(dataItem);
      listItem.listIndex = index;

      // If |listItems| is not on the list, appends it to the list and sets
      // the flag.
      if (!listItem.parentNode) {
        // @ts-ignore: error TS2339: Property 'appendChild' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.appendChild(listItem);
        isElementAppended[y] = true;
      }

      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItems_[index] = listItem;
      measuringItems.push(listItem);
    }

    // All mesurings must be placed after adding all the elements, to prevent
    // performance reducing.
    for (let y = 0; y < measuringIndexes.length; y++) {
      const index = measuringIndexes[y];
      // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not exist
      // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItemHeights_[index] =
          this.measureItemHeight_(measuringItems[y]);
    }

    // Removes all the temporary elements.
    for (let y = 0; y < measuringIndexes.length; y++) {
      // If the list item has been appended above, removes it.
      if (isElementAppended[y]) {
        // @ts-ignore: error TS2339: Property 'removeChild' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.removeChild(measuringItems[y]);
      }
    }
  },

  /**
   * Returns the height of after filler in the list.
   * @param {number} lastIndex The index of item past the last in viewport.
   * @return {number} The height of after filler.
   */
  getAfterFillerHeight(lastIndex) {
    if (this.fixedHeight_) {
      const itemHeight = this.getDefaultItemHeight_();
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      return (this.dataModel.length - lastIndex) * itemHeight;
    }

    let height = 0;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    for (let i = lastIndex; i < this.dataModel.length; i++) {
      height += this.getItemHeightByIndex_(i);
    }
    return height;
  },

  /**
   * Redraws the viewport.
   */
  redraw() {
    if (this.batchCount_ !== 0) {
      return;
    }

    const dataModel = this.dataModel;
    // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (!dataModel || !this.autoExpands_ && this.clientHeight === 0) {
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItems_ = {};
      // @ts-ignore: error TS2339: Property 'firstIndex_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.firstIndex_ = 0;
      // @ts-ignore: error TS2339: Property 'lastIndex_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.lastIndex_ = 0;
      // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.remainingSpace_ = this.clientHeight !== 0;
      this.mergeItems(0, 0);
      return;
    }

    // Save the previous positions before any manipulation of elements.
    // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const scrollTop = this.scrollTop;
    // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const clientHeight = this.clientHeight;

    // Store all the item sizes into the cache in advance, to prevent
    // interleave measuring with mutating dom.
    if (!this.fixedHeight_) {
      this.ensureAllItemSizesInCache();
    }

    // @ts-ignore: error TS6133: 'autoExpands' is declared but its value is
    // never read.
    const autoExpands = this.autoExpands_;

    const itemsInViewPort = this.getItemsInViewPort(scrollTop, clientHeight);
    // Draws the hidden rows just above/below the viewport to prevent
    // flashing in scroll.
    const firstIndex =
        // @ts-ignore: error TS2339: Property 'length' does not exist on type
        // 'never'.
        Math.max(0, Math.min(dataModel.length - 1, itemsInViewPort.first - 1));
    // @ts-ignore: error TS2339: Property 'length' does not exist on type
    // 'never'.
    const lastIndex = Math.min(itemsInViewPort.last + 1, dataModel.length);

    const beforeFillerHeight =
        this.autoExpands ? 0 : this.getItemTop(firstIndex);
    const afterFillerHeight =
        this.autoExpands ? 0 : this.getAfterFillerHeight(lastIndex);

    // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.beforeFiller_.style.height = beforeFillerHeight + 'px';

    const sm = this.selectionModel;
    const leadIndex = sm.leadIndex;

    // If the pinned item is hidden and it is not the lead item, then remove
    // it from cache. Note, that we restore the hidden status to false, since
    // the item is still in cache, and may be reused.
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.pinnedItem_ && this.pinnedItem_ !== this.cachedItems_[leadIndex]) {
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      if (this.pinnedItem_.hidden) {
        // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.removeChild(this.pinnedItem_);
        // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.pinnedItem_.hidden = false;
      }
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.pinnedItem_ = undefined;
    }

    this.mergeItems(firstIndex, lastIndex);

    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (!this.pinnedItem_ && this.cachedItems_[leadIndex] &&
        // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.cachedItems_[leadIndex].parentNode === this) {
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.pinnedItem_ = this.cachedItems_[leadIndex];
    }

    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.afterFiller_.style.height = afterFillerHeight + 'px';

    // Restores the number of pixels scrolled, since it might be changed while
    // DOM operations.
    // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.scrollTop = scrollTop;

    // We don't set the lead or selected properties until after adding all
    // items, in case they force relayout in response to these events.
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (leadIndex !== -1 && this.cachedItems_[leadIndex]) {
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.cachedItems_[leadIndex].lead = true;
    }
    for (let y = firstIndex; y < lastIndex; y++) {
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      if (sm.getIndexSelected(y) !== this.cachedItems_[y].selected) {
        // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.cachedItems_[y].selected = !this.cachedItems_[y].selected;
      }
    }

    // @ts-ignore: error TS2339: Property 'firstIndex_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.firstIndex_ = firstIndex;
    // @ts-ignore: error TS2339: Property 'lastIndex_' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.lastIndex_ = lastIndex;

    // @ts-ignore: error TS2339: Property 'length' does not exist on type
    // 'never'.
    this.remainingSpace_ = itemsInViewPort.last > dataModel.length;

    // Mesurings must be placed after adding all the elements, to prevent
    // performance reducing.
    if (!this.fixedHeight_) {
      for (let y = firstIndex; y < lastIndex; y++) {
        // @ts-ignore: error TS2339: Property 'cachedItemHeights_' does not
        // exist on type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.cachedItemHeights_[y] =
            // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist
            // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
            // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
            // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more
            // ...; startDragSelection(event: Event): void; }'.
            this.measureItemHeight_(this.cachedItems_[y]);
      }
    }
  },

  /**
   * Restore the lead item that is present in the list but may be updated
   * in the data model (supposed to be used inside a batch update). Usually
   * such an item would be recreated in the redraw method. If reinsertion
   * is undesirable (for instance to prevent losing focus) the item may be
   * updated and restored. Assumed the listItem relates to the same data item
   * as the lead item in the begin of the batch update.
   *
   * @param {ListItem} leadItem Already existing lead item.
   */
  restoreLeadItem(leadItem) {
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    delete this.cachedItems_[leadItem.listIndex];

    leadItem.listIndex = this.selectionModel.leadIndex;
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.pinnedItem_ = this.cachedItems_[leadItem.listIndex] = leadItem;
  },

  /**
   * Invalidates list by removing cached items.
   */
  invalidate() {
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItems_ = {};
    // @ts-ignore: error TS2339: Property 'cachedItemSized_' does not exist on
    // type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.cachedItemSized_ = {};
  },

  /**
   * Redraws a single item.
   * @param {number} index The row index to redraw.
   */
  redrawItem(index) {
    // @ts-ignore: error TS2339: Property 'firstIndex_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (index >= this.firstIndex_ &&
        // @ts-ignore: error TS2339: Property 'lastIndex_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        (index < this.lastIndex_ || this.remainingSpace_)) {
      // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      delete this.cachedItems_[index];
      this.redraw();
    }
  },

  /**
   * Called when a list item is activated, currently only by a double click
   * event.
   * @param {number} index The index of the activated item.
   */
  // @ts-ignore: error TS6133: 'index' is declared but its value is never read.
  activateItemAtIndex(index) {},

  /**
   * Returns a ListItem for the leadIndex. If the item isn't present in the
   * list creates it and inserts to the list (may be invisible if it's out of
   * the visible range).
   *
   * Item returned from this method won't be removed until it remains a lead
   * item or till the data model changes (unlike other items that could be
   * removed when they go out of the visible range).
   *
   * @return {ListItem} The lead item for the list.
   */
  ensureLeadItemExists() {
    const index = this.selectionModel.leadIndex;
    if (index < 0) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'ListItem'.
      return null;
    }
    // @ts-ignore: error TS2339: Property 'cachedItems_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const cachedItems = this.cachedItems_ || {};

    const item =
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        cachedItems[index] || this.createItem(this.dataModel.item(index));
    // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (this.pinnedItem_ !== item && this.pinnedItem_ &&
        // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on
        // type '{ __proto__: HTMLUListElement; measured_: Size | null;
        // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_:
        // boolean; itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
        // startDragSelection(event: Event): void; }'.
        this.pinnedItem_.hidden) {
      // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
      // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
      // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.removeChild(this.pinnedItem_);
    }
    // @ts-ignore: error TS2339: Property 'pinnedItem_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.pinnedItem_ = item;
    cachedItems[index] = item;
    item.listIndex = index;
    if (item.parentNode === this) {
      return item;
    }

    if (this.batchCount_ !== 0) {
      item.hidden = true;
    }

    // Item will get to the right place in redraw. Choose place to insert
    // reducing items reinsertion.
    // @ts-ignore: error TS2339: Property 'firstIndex_' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    if (index <= this.firstIndex_) {
      // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.insertBefore(item, this.beforeFiller_.nextSibling);
    } else {
      // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on
      // type '{ __proto__: HTMLUListElement; measured_: Size | null;
      // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
      // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
      // startDragSelection(event: Event): void; }'.
      this.insertBefore(item, this.afterFiller_);
    }
    this.redraw();
    return item;
  },

  /**
   * Starts drag selection by reacting 'dragstart' event.
   * @param {Event} event Event of dragstart.
   */
  startDragSelection(event) {
    event.preventDefault();
    const border = document.createElement('div');
    border.className = 'drag-selection-border';
    // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not exist
    // on type '{ __proto__: HTMLUListElement; measured_: Size | null;
    // autoExpands_: boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const rect = this.getBoundingClientRect();
    // @ts-ignore: error TS2339: Property 'scrollLeft' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const startX = event.clientX - rect.left + this.scrollLeft;
    // @ts-ignore: error TS2339: Property 'scrollTop' does not exist on type '{
    // __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    const startY = event.clientY - rect.top + this.scrollTop;
    border.style.left = startX + 'px';
    border.style.top = startY + 'px';
    // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
    const onMouseMove = function(event) {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const inRect = this.getBoundingClientRect();
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const x = event.clientX - inRect.left + this.scrollLeft;
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const y = event.clientY - inRect.top + this.scrollTop;
      border.style.left = Math.min(startX, x) + 'px';
      border.style.top = Math.min(startY, y) + 'px';
      border.style.width = Math.abs(startX - x) + 'px';
      border.style.height = Math.abs(startY - y) + 'px';
    }.bind(this);
    const onMouseUp = function() {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      this.removeChild(border);
      document.removeEventListener('mousemove', onMouseMove, true);
      document.removeEventListener('mouseup', onMouseUp, true);
    }.bind(this);
    document.addEventListener('mousemove', onMouseMove, true);
    document.addEventListener('mouseup', onMouseUp, true);
    // @ts-ignore: error TS2339: Property 'appendChild' does not exist on type
    // '{ __proto__: HTMLUListElement; measured_: Size | null; autoExpands_:
    // boolean; fixedHeight_: boolean; remainingSpace_: boolean;
    // itemConstructor_: (arg0: any) => ListItem; ... 56 more ...;
    // startDragSelection(event: Event): void; }'.
    this.appendChild(border);
  },
};

/** @type {boolean} */
List.prototype.disabled;
Object.defineProperty(
    List.prototype, 'disabled',
    getPropertyDescriptor('disabled', PropertyKind.BOOL_ATTR));

/**
 * Whether the list or one of its descendents has focus. This is necessary
 * because list items can contain controls that can be focused, and for some
 * purposes (e.g., styling), the list can still be conceptually focused at
 * that point even though it doesn't actually have the page focus.
 * @type {boolean}
 */
List.prototype.hasElementFocus;
Object.defineProperty(
    List.prototype, 'hasElementFocus',
    getPropertyDescriptor('hasElementFocus', PropertyKind.BOOL_ATTR));

/**
 * Mousedown event handler.
 * @this {List}
 * @param {Event} e The mouse event object.
 */
function handleMouseDown(e) {
  const target = /** @type {!HTMLElement} */ (e.target);
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'List'.
  const listItem = this.getListItemAncestor(target);
  const wasSelected = listItem && listItem.selected;
  // @ts-ignore: error TS2339: Property 'handlePointerDownUp_' does not exist on
  // type 'List'.
  this.handlePointerDownUp_(e);

  // @ts-ignore: error TS2339: Property 'button' does not exist on type 'Event'.
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
 * @this {List}
 * @param {Event} e The event object for 'dragstart'.
 */
function handleDragStart(e) {
  e = /** @type {MouseEvent} */ (e);
  // @ts-ignore: error TS2339: Property 'clientY' does not exist on type
  // 'Event'.
  const element = e.target.ownerDocument.elementFromPoint(e.clientX, e.clientY);
  // @ts-ignore: error TS2339: Property 'getListItemAncestor' does not exist on
  // type 'List'.
  const listItem = this.getListItemAncestor(element);
  if (!listItem) {
    return;
  }

  // @ts-ignore: error TS2339: Property 'getIndexOfListItem' does not exist on
  // type 'List'.
  const index = this.getIndexOfListItem(listItem);
  if (index === -1) {
    return;
  }

  // @ts-ignore: error TS2339: Property 'selectionModel_' does not exist on type
  // 'List'.
  const isAlreadySelected = this.selectionModel_.getIndexSelected(index);
  if (!isAlreadySelected) {
    // @ts-ignore: error TS2339: Property 'selectionModel_' does not exist on
    // type 'List'.
    this.selectionModel_.selectedIndex = index;
  }
}

/**
 * Check if |start| or its ancestor under |root| is focusable.
 * This is a helper for handleMouseDown.
 * @param {!Element} start An element which we start to check.
 * @param {!Element} root An element which we finish to check.
 * @return {boolean} True if we found a focusable element.
 */
function containsFocusableElement(start, root) {
  for (let element = start; element && element !== root;
       // @ts-ignore: error TS2322: Type 'HTMLElement | null' is not assignable
       // to type 'Element'.
       element = element.parentElement) {
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'Element'.
    if (element.tabIndex >= 0 && !element.disabled) {
      return true;
    }
  }
  return false;
}
