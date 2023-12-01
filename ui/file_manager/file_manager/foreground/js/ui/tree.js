// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent, getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

/**
 * The number of pixels to indent per level.
 * @type {number}
 * @const
 */
const INDENT = 20;

/**
 * Decorates elements as an instance of a class.
 * @param {string|!Element} source The way to find the element(s) to decorate.
 *     If this is a string then {@code querySeletorAll} is used to find the
 *     elements to decorate.
 * @param {!Function} constr The constructor to decorate with. The constr
 *     needs to have a {@code decorate} function.
 * @closurePrimitive {asserts.matchesReturn}
 */
export function decorate(source, constr) {
  let elements;
  if (typeof source === 'string') {
    elements = document.querySelectorAll(source);
  } else {
    elements = [source];
  }

  for (const el of elements) {
    if (!(el instanceof constr)) {
      // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
      // 'Function'.
      constr.decorate(el);
    }
  }
}

/**
 * Helper function for creating new element for define.
 */
// @ts-ignore: error TS7006: Parameter 'opt_bag' implicitly has an 'any' type.
function createElementHelper(tagName, opt_bag) {
  // Allow passing in ownerDocument to create in a different document.
  let doc;
  if (opt_bag && opt_bag.ownerDocument) {
    doc = opt_bag.ownerDocument;
  } else {
    doc = document;
  }
  return doc.createElement(tagName);
}

/**
 * Creates the constructor for a UI element class.
 *
 * Usage:
 * <pre>
 * var List = cr.ui.define('list');
 * List.prototype = {
 *   __proto__: HTMLUListElement.prototype,
 *   decorate() {
 *     ...
 *   },
 *   ...
 * };
 * </pre>
 *
 * @param {string|Function} tagNameOrFunction The tagName or
 *     function to use for newly created elements. If this is a function it
 *     needs to return a new element when called.
 * @return {function(Record<string, any>=):HTMLElement} The constructor function
 *     which takes an optional property bag. The function also has a static
 *     {@code decorate} method added to it.
 */
export function define(tagNameOrFunction) {
  /** @type {Function} */
  let createFunction;
  /** @type {string} */
  let tagName;
  if (typeof tagNameOrFunction === 'function') {
    createFunction = tagNameOrFunction;
    tagName = '';
  } else {
    createFunction = createElementHelper;
    tagName = tagNameOrFunction;
  }

  /**
   * Creates a new UI element constructor.
   * @param {Record<string, any>=} opt_propertyBag Optional bag of properties to
   *     set on the object after created. The property {@code ownerDocument} is
   *     special cased and it allows you to create the element in a different
   *     document than the default.
   */
  function f(opt_propertyBag) {
    /** @type {HTMLElement} */
    const el = createFunction(tagName, opt_propertyBag);
    f.decorate(el);
    for (const propertyName in opt_propertyBag) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type
      // because expression of type 'string' can't be used to index type
      // 'Object'.
      el[propertyName] = opt_propertyBag[propertyName];
    }
    return el;
  }

  /**
   * Decorates an element as a UI element class.
   * @param {!HTMLElement} el The element to decorate.
   */
  f.decorate = function(el) {
    if (f.prototype.isPrototypeOf(el)) {
      return;
    }

    Object.setPrototypeOf(el, f.prototype);
    if ('decorate' in el) {
      // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
      // 'Element'.
      el.decorate();
    }
  };

  return f;
}

/**
 * Input elements do not grow and shrink with their content. This is a simple
 * (and not very efficient) way of handling shrinking to content with support
 * for min width and limited by the width of the parent element.
 * @param el {HTMLElement} The element to limit the width for.
 * @param parentEl {HTMLElement} The parent element that should limit the size.
 * @param min {number} The minimum width.
 * @param scale {number=} Optional scale factor to apply to the width.
 */
export function limitInputWidth(el, parentEl, min, scale) {
  // Needs a size larger than borders
  el.style.width = '9px';
  const doc = el.ownerDocument;
  const win = /** @type {Window} */ (doc.defaultView);
  const computedStyle = win.getComputedStyle(el);
  const parentComputedStyle = win.getComputedStyle(parentEl);
  const rtl = computedStyle.direction === 'rtl';

  // To get the max width we get the width of the treeItem minus the position
  // of the input.
  const inputRect = el.getBoundingClientRect();  // box-sizing
  const parentRect = parentEl.getBoundingClientRect();
  const startPos = rtl ? parentRect.right - inputRect.right :
                         inputRect.left - parentRect.left;

  // Add up border and padding of the input.
  const inner = parseInt(computedStyle.borderLeftWidth, 9) +
      parseInt(computedStyle.paddingLeft, 9) +
      parseInt(computedStyle.paddingRight, 9) +
      parseInt(computedStyle.borderRightWidth, 9);

  // We also need to subtract the padding of parent to prevent it to overflow.
  const parentPadding = rtl ? parseInt(parentComputedStyle.paddingLeft, 9) :
                              parseInt(parentComputedStyle.paddingRight, 9);

  let max = parentEl.clientWidth - startPos - inner - parentPadding;
  if (scale) {
    max *= scale;
  }

  function limit() {
    if (el.scrollWidth > max) {
      el.style.width = max + 'px';
    } else {
      el.style.width = '-1';
      const sw = el.scrollWidth;
      if (sw < min) {
        el.style.width = min + 'px';
      } else {
        el.style.width = sw + 'px';
      }
    }
  }

  el.addEventListener('input', limit);
  limit();
}

/**
 * A custom rowElement depth (indent) style handler where undefined uses the
 * default depth INDENT styling, see TreeItem.setDepth_().
 *
 * @type {?function(!TreeItem,number):void}
 */
let customRowElementDepthStyleHandler = null;

/**
 * Returns the computed style for an element.
 * @param {!Element} el The element to get the computed style for.
 * @return {!CSSStyleDeclaration} The computed style.
 */
function getComputedStyle(el) {
  // @ts-ignore: error TS18047: 'el.ownerDocument.defaultView' is possibly
  // 'null'.
  return assert(el.ownerDocument.defaultView.getComputedStyle(el));
}

/**
 * Helper function that finds the first ancestor tree item.
 * @param {Node} node The node to start searching from.
 * @return {TreeItem} The found tree item or null if not found.
 */
function findTreeItem(node) {
  while (node && !(node instanceof TreeItem)) {
    // @ts-ignore: error TS2322: Type 'ParentNode | null' is not assignable to
    // type 'Node'.
    node = node.parentNode;
  }
  return node;
}

/**
 * Creates a new tree element.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLElement}
 */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const Tree = define('tree');

Tree.prototype = {
  __proto__: HTMLElement.prototype,

  /**
   * Initializes the element.
   */
  decorate() {
    // Make list focusable
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'.
    if (!this.hasAttribute('tabindex')) {
      // @ts-ignore: error TS2339: Property 'tabIndex' does not exist on type '{
      // __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
      // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items:
      // any; add(treeItem: TreeItem): void; ... 8 more ...;
      // getRectForContextMenu(): ClientRect; }'.
      this.tabIndex = 0;
    }

    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void;
    // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
    // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8 more
    // ...; getRectForContextMenu(): ClientRect; }'.
    this.addEventListener('click', this.handleClick);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void;
    // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
    // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8 more
    // ...; getRectForContextMenu(): ClientRect; }'.
    this.addEventListener('mousedown', this.handleMouseDown);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void;
    // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
    // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8 more
    // ...; getRectForContextMenu(): ClientRect; }'.
    this.addEventListener('dblclick', this.handleDblClick);
    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void;
    // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
    // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8 more
    // ...; getRectForContextMenu(): ClientRect; }'.
    this.addEventListener('keydown', this.handleKeyDown);

    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'.
    if (!this.hasAttribute('role')) {
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void;
      // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
      // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8
      // more ...; getRectForContextMenu(): ClientRect; }'.
      this.setAttribute('role', 'tree');
    }
  },

  /**
   * Returns the tree item rowElement style handler.
   *
   * @return {function(!TreeItem,number)|undefined}
   */
  get rowElementDepthStyleHandler() {
    // @ts-ignore: error TS2322: Type '((arg0: TreeItem, arg1: number) => void)
    // | null' is not assignable to type '((arg0: TreeItem, arg1: number) =>
    // any) | undefined'.
    return customRowElementDepthStyleHandler;
  },

  /**
   * Sets a tree item rowElement style handler, which allows Tree users to
   * customize the depth (indent) style of tree item rowElements.
   *
   * @param {function(!TreeItem,number)|undefined} handler
   */
  set rowElementDepthStyleHandler(handler) {
    // @ts-ignore: error TS2322: Type '((arg0: TreeItem, arg1: number) => any) |
    // undefined' is not assignable to type '((arg0: TreeItem, arg1: number) =>
    // void) | null'.
    customRowElementDepthStyleHandler = handler;
  },

  /**
   * Returns the tree item that are children of this tree.
   */
  // @ts-ignore: error TS7023: 'items' implicitly has return type 'any' because
  // it does not have a return type annotation and is referenced directly or
  // indirectly in one of its return expressions.
  get items() {
    // @ts-ignore: error TS2339: Property 'children' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'.
    return this.children;
  },

  /**
   * Adds a tree item to the tree.
   * @param {!TreeItem} treeItem The item to add.
   */
  add(treeItem) {
    this.addAt(treeItem, 0xffffffff);
  },

  /**
   * Adds a tree item at the given index.
   * @param {!TreeItem} treeItem The item to add.
   * @param {number} index The index where we want to add the item.
   */
  addAt(treeItem, index) {
    // @ts-ignore: error TS2339: Property 'children' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'.
    this.insertBefore(treeItem, this.children[index]);
    // @ts-ignore: error TS2339: Property 'setDepth_' does not exist on type
    // 'TreeItem'.
    treeItem.setDepth_(this.depth + 1);
  },

  /**
   * Removes a tree item child.
   *
   * TODO(dbeam): this method now conflicts with HTMLElement#remove(), which
   * is why the @param is optional. Rename.
   *
   * @param {!TreeItem=} treeItem The tree item to remove.
   */
  remove(treeItem) {
    // @ts-ignore: error TS2339: Property 'removeChild' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'.
    this.removeChild(/** @type {!TreeItem} */ (treeItem));
  },

  /**
   * The depth of the node. This is 0 for the tree itself.
   * @type {number}
   */
  get depth() {
    return 0;
  },

  /**
   * Handles click events on the tree and forwards the event to the relevant
   * tree items as necessary.
   * @param {Event} e The click event object.
   */
  handleClick(e) {
    const treeItem = findTreeItem(/** @type {!Node} */ (e.target));
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'TreeItem'.
    if (treeItem && !treeItem.disabled) {
      // @ts-ignore: error TS2339: Property 'handleClick' does not exist on type
      // 'TreeItem'.
      treeItem.handleClick(e);
    }
  },

  // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
  handleMouseDown(e) {
    if (e.button === 2) {  // right
      this.handleClick(e);
    }
  },

  /**
   * Handles double click events on the tree.
   * @param {Event} e The dblclick event object.
   */
  handleDblClick(e) {
    const treeItem = findTreeItem(/** @type {!Node} */ (e.target));
    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'TreeItem'.
    if (treeItem && !treeItem.disabled) {
      // @ts-ignore: error TS2339: Property 'expanded' does not exist on type
      // 'TreeItem'.
      treeItem.expanded = !treeItem.expanded;
    }
  },

  /**
   * Handles keydown events on the tree and updates selection and exanding
   * of tree items.
   * @param {Event} e The click event object.
   */
  handleKeyDown(e) {
    let itemToSelect;
    // @ts-ignore: error TS2339: Property 'ctrlKey' does not exist on type
    // 'Event'.
    if (e.ctrlKey) {
      return;
    }

    const item = this.selectedItem;
    if (!item) {
      return;
    }

    // @ts-ignore: error TS2345: Argument of type 'TreeItem' is not assignable
    // to parameter of type 'Element'.
    const rtl = getComputedStyle(item).direction === 'rtl';

    const selectableItems = [];
    for (const item of this.items) {
      if (!item.disabled) {
        selectableItems.push(item);
      }
    }
    if (selectableItems.length === 0) {
      return;
    }

    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    switch (e.key) {
      case 'ArrowUp':
        itemToSelect = item ? getPrevious(item) :
                              selectableItems[selectableItems.length - 1];
        break;
      case 'ArrowDown':
        itemToSelect = item ? getNext(item) : selectableItems[0];
        break;
      case 'ArrowLeft':
      case 'ArrowRight':
        // Don't let back/forward keyboard shortcuts be used.
        // @ts-ignore: error TS2339: Property 'altKey' does not exist on type
        // 'Event'.
        if (e.altKey) {
          break;
        }

        // @ts-ignore: error TS2339: Property 'key' does not exist on type
        // 'Event'.
        if (e.key === 'ArrowLeft' && !rtl || e.key === 'ArrowRight' && rtl) {
          // @ts-ignore: error TS2339: Property 'expanded' does not exist on
          // type 'TreeItem'.
          if (item.expanded) {
            // @ts-ignore: error TS2339: Property 'expanded' does not exist on
            // type 'TreeItem'.
            item.expanded = false;
          } else {
            // @ts-ignore: error TS2339: Property 'parentNode' does not exist on
            // type 'TreeItem'.
            itemToSelect = findTreeItem(item.parentNode);
          }
        } else {
          // @ts-ignore: error TS2339: Property 'expanded' does not exist on
          // type 'TreeItem'.
          if (!item.expanded) {
            // @ts-ignore: error TS2339: Property 'expanded' does not exist on
            // type 'TreeItem'.
            item.expanded = true;
          } else {
            // @ts-ignore: error TS2339: Property 'items' does not exist on type
            // 'TreeItem'.
            itemToSelect = item.items[0];
          }
        }
        break;
      case 'Home':
        itemToSelect = selectableItems[0];
        break;
      case 'End':
        itemToSelect = selectableItems[selectableItems.length - 1];
        break;
    }

    if (itemToSelect) {
      itemToSelect.selected = true;
      e.preventDefault();
    }
  },

  /**
   * The selected tree item or null if none.
   * @type {TreeItem}
   */
  get selectedItem() {
    // @ts-ignore: error TS2551: Property 'selectedItem_' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'. Did you mean 'selectedItem'?
    return this.selectedItem_ || null;
  },
  set selectedItem(item) {
    // @ts-ignore: error TS2551: Property 'selectedItem_' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; rowElementDepthStyleHandler:
    // ((arg0: TreeItem, arg1: number) => any) | undefined; readonly items: any;
    // add(treeItem: TreeItem): void; ... 8 more ...; getRectForContextMenu():
    // ClientRect; }'. Did you mean 'selectedItem'?
    const oldSelectedItem = this.selectedItem_;
    if (oldSelectedItem !== item) {
      // Set the selectedItem_ before deselecting the old item since we only
      // want one change when moving between items.
      // @ts-ignore: error TS2551: Property 'selectedItem_' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void;
      // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
      // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8
      // more ...; getRectForContextMenu(): ClientRect; }'. Did you mean
      // 'selectedItem'?
      this.selectedItem_ = item;

      if (oldSelectedItem) {
        oldSelectedItem.selected = false;
      }

      if (item) {
        // @ts-ignore: error TS2339: Property 'selected' does not exist on type
        // 'TreeItem'.
        item.selected = true;
        // @ts-ignore: error TS2339: Property 'id' does not exist on type
        // 'TreeItem'.
        if (item.id) {
          // @ts-ignore: error TS2339: Property 'id' does not exist on type
          // 'TreeItem'.
          this.setAttribute('aria-activedescendant', item.id);
        }
      } else {
        // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist
        // on type '{ __proto__: HTMLElement; decorate(): void;
        // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any)
        // | undefined; readonly items: any; add(treeItem: TreeItem): void; ...
        // 8 more ...; getRectForContextMenu(): ClientRect; }'.
        this.removeAttribute('aria-activedescendant');
      }
      // @ts-ignore: error TS2345: Argument of type '{ __proto__: HTMLElement;
      // decorate(): void; rowElementDepthStyleHandler: ((arg0: TreeItem, arg1:
      // number) => any) | undefined; readonly items: any; add(treeItem:
      // TreeItem): void; ... 8 more ...; getRectForContextMenu(): ClientRect;
      // }' is not assignable to parameter of type 'EventTarget'.
      dispatchSimpleEvent(this, 'change');
    }
  },

  /**
   * @return {!ClientRect} The rect to use for the context menu.
   */
  getRectForContextMenu() {
    // TODO(arv): Add trait support so we can share more code between trees
    // and lists.
    if (this.selectedItem) {
      // @ts-ignore: error TS2339: Property 'rowElement' does not exist on type
      // 'TreeItem'.
      return this.selectedItem.rowElement.getBoundingClientRect();
    }
    // @ts-ignore: error TS2339: Property 'getBoundingClientRect' does not exist
    // on type '{ __proto__: HTMLElement; decorate(): void;
    // rowElementDepthStyleHandler: ((arg0: TreeItem, arg1: number) => any) |
    // undefined; readonly items: any; add(treeItem: TreeItem): void; ... 8 more
    // ...; getRectForContextMenu(): ClientRect; }'.
    return this.getBoundingClientRect();
  },
};

/**
 * Determines the visibility of icons next to the treeItem labels. If set to
 * 'hidden', no space is reserved for icons and no icons are displayed next
 * to treeItem labels. If set to 'parent', folder icons will be displayed
 * next to expandable parent nodes. If set to 'all' folder icons will be
 * displayed next to all nodes. Icons can be set using the treeItem's icon
 * property.
 * @type {boolean}
 */
Tree.prototype.iconVisibility;
Object.defineProperty(
    Tree.prototype, 'iconVisibility',
    getPropertyDescriptor('iconVisibility', PropertyKind.ATTR));

/**
 * Incremental counter for an auto generated ID of the tree item. This will
 * be incremented per element, so each element never share same ID.
 *
 * @type {number}
 */
let treeItemAutoGeneratedIdCounter = 0;

/**
 * This is used as a blueprint for new tree item elements.
 * @type {!HTMLElement}
 */
const treeItemProto = (function() {
  const treeItem = document.createElement('div');
  treeItem.className = 'tree-item';
  treeItem.innerHTML = getTrustedHTML`<div class="tree-row">
      <span class="expand-icon"></span>
      <span class="tree-label-icon"></span>
      <span class="tree-label"></span>
      </div>
      <div class="tree-children" role="group"></div>`;


  treeItem.setAttribute('role', 'treeitem');
  return treeItem;
})();

/**
 * Creates a new tree item.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLElement}
 */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const TreeItem = define(function() {
  const treeItem = treeItemProto.cloneNode(true);
  // @ts-ignore: error TS2339: Property 'id' does not exist on type 'Node'.
  treeItem.id = 'tree-item-autogen-id-' + treeItemAutoGeneratedIdCounter++;
  return treeItem;
});

TreeItem.prototype = {
  __proto__: HTMLElement.prototype,

  /**
   * Initializes the element.
   */
  decorate() {
    const labelId =
        'tree-item-label-autogen-id-' + treeItemAutoGeneratedIdCounter;
    this.labelElement.id = labelId;
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    this.setAttribute('aria-labelledby', labelId);
  },

  /**
   * The tree items children.
   */
  // @ts-ignore: error TS7023: 'items' implicitly has return type 'any' because
  // it does not have a return type annotation and is referenced directly or
  // indirectly in one of its return expressions.
  get items() {
    // @ts-ignore: error TS2339: Property 'lastElementChild' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    return this.lastElementChild.children;
  },

  /**
   * The depth of the tree item.
   * @type {number}
   */
  depth_: 0,
  get depth() {
    return this.depth_;
  },

  /**
   * Sets the depth.
   * @param {number} depth The new depth.
   * @private
   */
  setDepth_(depth) {
    if (depth !== this.depth_) {
      const rowDepth = Math.max(0, depth - 1);
      if (!customRowElementDepthStyleHandler) {
        this.rowElement.style.paddingInlineStart = rowDepth * INDENT + 'px';
      } else {
        customRowElementDepthStyleHandler(this, rowDepth);
      }

      this.depth_ = depth;
      const items = this.items;
      for (let i = 0, item; item = items[i]; i++) {
        item.setDepth_(depth + 1);
      }
    }
  },

  /**
   * Whether this tree item is disabled for selection.
   * @type {boolean}
   */
  get disabled() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    return this.hasAttribute('disabled');
  },
  set disabled(b) {
    // @ts-ignore: error TS2339: Property 'toggleAttribute' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    this.toggleAttribute('disabled', b);
  },

  /**
   * Adds a tree item as a child.
   * @param {!TreeItem} child The child to add.
   */
  add(child) {
    this.addAt(child, 0xffffffff);
  },

  /**
   * Adds a tree item as a child at a given index.
   * @param {!TreeItem} child The child to add.
   * @param {number} index The index where to add the child.
   */
  addAt(child, index) {
    // @ts-ignore: error TS2339: Property 'lastElementChild' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    this.lastElementChild.insertBefore(child, this.items[index]);
    if (this.items.length === 1) {
      this.hasChildren = true;
    }
    // @ts-ignore: error TS2339: Property 'setDepth_' does not exist on type
    // 'TreeItem'.
    child.setDepth_(this.depth + 1);
  },

  /**
   * Removes a child.
   * @param {!TreeItem=} child The tree item child to remove.
   * @override
   */
  remove(child) {
    // If we removed the selected item we should become selected.
    const tree = this.tree;
    // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on type
    // 'Tree'.
    const selectedItem = tree.selectedItem;
    // @ts-ignore: error TS2339: Property 'contains' does not exist on type
    // 'TreeItem'.
    if (selectedItem && child.contains(selectedItem)) {
      this.selected = true;
    }

    // @ts-ignore: error TS2339: Property 'lastElementChild' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    this.lastElementChild.removeChild(/** @type {!TreeItem} */ (child));
    if (this.items.length === 0) {
      this.hasChildren = false;
    }
  },

  /**
   * The parent tree item.
   * @type {!Tree|TreeItem}
   */
  get parentItem() {
    // @ts-ignore: error TS2339: Property 'parentNode' does not exist on type '{
    // __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    let p = this.parentNode;
    while (p && !(p instanceof TreeItem) && !(p instanceof Tree)) {
      p = p.parentNode;
    }
    return p;
  },

  /**
   * The tree that the tree item belongs to or null of no added to a tree.
   * @type {Tree}
   */
  get tree() {
    let t = this.parentItem;
    while (t && !(t instanceof Tree)) {
      // @ts-ignore: error TS2339: Property 'parentItem' does not exist on type
      // 'TreeItem | Tree'.
      t = t.parentItem;
    }
    // @ts-ignore: error TS2322: Type 'TreeItem | Tree' is not assignable to
    // type 'Tree'.
    return t;
  },

  /**
   * Whether the tree item is expanded or not.
   * @type {boolean}
   */
  get expanded() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    return this.hasAttribute('expanded');
  },
  set expanded(b) {
    if (this.expanded === b) {
      return;
    }

    // @ts-ignore: error TS2339: Property 'lastElementChild' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    const treeChildren = this.lastElementChild;

    if (b) {
      if (this.mayHaveChildren_) {
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type '{ __proto__: HTMLElement; decorate(): void; readonly items:
        // any; depth_: number; readonly depth: number; setDepth_(depth:
        // number): void; disabled: boolean; add(child: TreeItem): void; ... 13
        // more ...; editing: any; }'.
        this.setAttribute('expanded', '');
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type '{ __proto__: HTMLElement; decorate(): void; readonly items:
        // any; depth_: number; readonly depth: number; setDepth_(depth:
        // number): void; disabled: boolean; add(child: TreeItem): void; ... 13
        // more ...; editing: any; }'.
        this.setAttribute('aria-expanded', 'true');
        treeChildren.setAttribute('expanded', '');
        // @ts-ignore: error TS2345: Argument of type '{ __proto__: HTMLElement;
        // decorate(): void; readonly items: any; depth_: number; readonly
        // depth: number; setDepth_(depth: number): void; disabled: boolean;
        // add(child: TreeItem): void; ... 13 more ...; editing: any; }' is not
        // assignable to parameter of type 'EventTarget'.
        dispatchSimpleEvent(this, 'expand', true);
        // @ts-ignore: error TS2339: Property 'scrollIntoViewIfNeeded' does not
        // exist on type '{ __proto__: HTMLElement; decorate(): void; readonly
        // items: any; depth_: number; readonly depth: number; setDepth_(depth:
        // number): void; disabled: boolean; add(child: TreeItem): void; ... 13
        // more ...; editing: any; }'.
        this.scrollIntoViewIfNeeded(false);
      }
    } else {
      const tree = this.tree;
      if (tree && !this.selected) {
        // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on
        // type 'Tree'.
        const oldSelected = tree.selectedItem;
        // @ts-ignore: error TS2339: Property 'contains' does not exist on type
        // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
        // depth_: number; readonly depth: number; setDepth_(depth: number):
        // void; disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
        // editing: any; }'.
        if (oldSelected && this.contains(oldSelected)) {
          this.selected = true;
        }
      }
      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.removeAttribute('expanded');
      if (this.mayHaveChildren_) {
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type '{ __proto__: HTMLElement; decorate(): void; readonly items:
        // any; depth_: number; readonly depth: number; setDepth_(depth:
        // number): void; disabled: boolean; add(child: TreeItem): void; ... 13
        // more ...; editing: any; }'.
        this.setAttribute('aria-expanded', 'false');
      } else {
        // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist
        // on type '{ __proto__: HTMLElement; decorate(): void; readonly items:
        // any; depth_: number; readonly depth: number; setDepth_(depth:
        // number): void; disabled: boolean; add(child: TreeItem): void; ... 13
        // more ...; editing: any; }'.
        this.removeAttribute('aria-expanded');
      }
      treeChildren.removeAttribute('expanded');
      // @ts-ignore: error TS2345: Argument of type '{ __proto__: HTMLElement;
      // decorate(): void; readonly items: any; depth_: number; readonly depth:
      // number; setDepth_(depth: number): void; disabled: boolean; add(child:
      // TreeItem): void; ... 13 more ...; editing: any; }' is not assignable to
      // parameter of type 'EventTarget'.
      dispatchSimpleEvent(this, 'collapse', true);
    }
  },

  /**
   * Expands all parent items.
   */
  reveal() {
    let pi = this.parentItem;
    while (pi && !(pi instanceof Tree)) {
      // @ts-ignore: error TS2339: Property 'expanded' does not exist on type
      // 'TreeItem | Tree'.
      pi.expanded = true;
      // @ts-ignore: error TS2339: Property 'parentItem' does not exist on type
      // 'TreeItem | Tree'.
      pi = pi.parentItem;
    }
  },

  /**
   * The element representing the row that gets highlighted.
   * @type {!HTMLElement}
   */
  get rowElement() {
    // @ts-ignore: error TS2339: Property 'firstElementChild' does not exist on
    // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
    // depth_: number; readonly depth: number; setDepth_(depth: number): void;
    // disabled: boolean; add(child: TreeItem): void; ... 13 more ...; editing:
    // any; }'.
    return this.firstElementChild;
  },

  /**
   * The element containing the label text.
   * @type {!HTMLElement}
   */
  get labelElement() {
    // @ts-ignore: error TS2322: Type 'Element | null' is not assignable to type
    // 'HTMLElement'.
    return this.rowElement.lastElementChild;
  },

  /**
   * The label text.
   * @type {string}
   */
  get label() {
    // @ts-ignore: error TS2322: Type 'string | null' is not assignable to type
    // 'string'.
    return this.labelElement.textContent;
  },
  set label(s) {
    this.labelElement.textContent = s;
  },

  /**
   * Whether the tree item is selected or not.
   * @type {boolean}
   */
  get selected() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    return this.hasAttribute('selected');
  },
  set selected(b) {
    if (this.selected === b) {
      return;
    }
    const rowItem = this.rowElement;
    const tree = this.tree;
    if (b) {
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.setAttribute('selected', '');
      rowItem.setAttribute('selected', '');
      this.reveal();
      // @ts-ignore: error TS2339: Property 'scrollIntoViewIfNeeded' does not
      // exist on type 'HTMLElement'.
      this.labelElement.scrollIntoViewIfNeeded(false);
      if (tree) {
        // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on
        // type 'Tree'.
        tree.selectedItem = this;
      }
    } else {
      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.removeAttribute('selected');
      rowItem.removeAttribute('selected');
      // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on
      // type 'Tree'.
      if (tree && tree.selectedItem === this) {
        // @ts-ignore: error TS2339: Property 'selectedItem' does not exist on
        // type 'Tree'.
        tree.selectedItem = null;
      }
    }
  },

  /**
   * Whether the tree item has children.
   * @type {boolean}
   */
  get mayHaveChildren_() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    return this.hasAttribute('may-have-children');
  },
  set mayHaveChildren_(b) {
    const rowItem = this.rowElement;
    if (b) {
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.setAttribute('may-have-children', '');
      rowItem.setAttribute('may-have-children', '');
    } else {
      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.removeAttribute('may-have-children');
      rowItem.removeAttribute('may-have-children');
    }
  },

  /**
   * Whether the tree item has children.
   * @type {boolean}
   */
  get hasChildren() {
    return !!this.items[0];
  },

  /**
   * Whether the tree item has children.
   * @type {boolean}
   */
  set hasChildren(b) {
    const rowItem = this.rowElement;
    // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    this.setAttribute('has-children', b);
    // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
    // parameter of type 'string'.
    rowItem.setAttribute('has-children', b);
    if (b) {
      this.mayHaveChildren_ = true;
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.setAttribute('aria-expanded', this.expanded);
    }
  },

  /**
   * Called when the user clicks on a tree item. This is forwarded from the
   * Tree.
   * @param {Event} e The click event.
   */
  handleClick(e) {
    // @ts-ignore: error TS2339: Property 'className' does not exist on type
    // 'EventTarget'.
    if (e.target.className === 'expand-icon') {
      this.expanded = !this.expanded;
    } else {
      this.selected = true;
    }
  },

  /**
   * Makes the tree item user editable. If the user renamed the item a
   * bubbling {@code rename} event is fired.
   * @type {boolean}
   */
  set editing(editing) {
    const oldEditing = this.editing;
    if (editing === oldEditing) {
      return;
    }

    const self = this;
    const labelEl = this.labelElement;
    const text = this.label;
    // @ts-ignore: error TS7034: Variable 'input' implicitly has type 'any' in
    // some locations where its type cannot be determined.
    let input;

    // Handles enter and escape which trigger reset and commit respectively.
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    function handleKeydown(e) {
      // Make sure that the tree does not handle the key.
      e.stopPropagation();

      // Calling tree.focus blurs the input which will make the tree item
      // non editable.
      switch (e.key) {
        case 'Escape':
          // @ts-ignore: error TS7005: Variable 'input' implicitly has an 'any'
          // type.
          input.value = text;
        // fall through
        case 'Enter':
          // @ts-ignore: error TS2339: Property 'focus' does not exist on type
          // 'Tree'.
          self.tree.focus();
      }
    }

    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    function stopPropagation(e) {
      e.stopPropagation();
    }

    if (editing) {
      this.selected = true;
      // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.setAttribute('editing', '');
      // @ts-ignore: error TS2339: Property 'draggable' does not exist on type
      // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.draggable = false;

      // We create an input[type=text] and copy over the label value. When
      // the input loses focus we set editing to false again.
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      input = this.ownerDocument.createElement('input');
      input.value = text;
      if (labelEl.firstChild) {
        labelEl.replaceChild(input, labelEl.firstChild);
      } else {
        labelEl.appendChild(input);
      }

      input.addEventListener('keydown', handleKeydown);
      input.addEventListener('blur', () => {
        this.editing = false;
      });

      // Make sure that double clicks do not expand and collapse the tree
      // item.
      const eventsToStop = ['mousedown', 'mouseup', 'contextmenu', 'dblclick'];
      eventsToStop.forEach(function(type) {
        // @ts-ignore: error TS7005: Variable 'input' implicitly has an 'any'
        // type.
        input.addEventListener(type, stopPropagation);
      });

      // Wait for the input element to recieve focus before sizing it.
      const rowElement = this.rowElement;
      const onFocus = function() {
        // @ts-ignore: error TS7005: Variable 'input' implicitly has an 'any'
        // type.
        input.removeEventListener('focus', onFocus);
        // 20 = the padding and border of the tree-row
        // @ts-ignore: error TS7005: Variable 'input' implicitly has an 'any'
        // type.
        limitInputWidth(input, rowElement, 100);
      };
      input.addEventListener('focus', onFocus);
      input.focus();
      input.select();

      // @ts-ignore: error TS2339: Property 'oldLabel_' does not exist on type
      // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.oldLabel_ = text;
    } else {
      // @ts-ignore: error TS2339: Property 'removeAttribute' does not exist on
      // type '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.removeAttribute('editing');
      // @ts-ignore: error TS2339: Property 'draggable' does not exist on type
      // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      this.draggable = true;
      input = labelEl.firstChild;
      // @ts-ignore: error TS2339: Property 'value' does not exist on type
      // 'ChildNode'.
      const value = input.value;
      if (/^\s*$/.test(value)) {
        // @ts-ignore: error TS2339: Property 'oldLabel_' does not exist on type
        // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
        // depth_: number; readonly depth: number; setDepth_(depth: number):
        // void; disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
        // editing: any; }'.
        labelEl.textContent = this.oldLabel_;
      } else {
        labelEl.textContent = value;
        // @ts-ignore: error TS2339: Property 'oldLabel_' does not exist on type
        // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
        // depth_: number; readonly depth: number; setDepth_(depth: number):
        // void; disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
        // editing: any; }'.
        if (value !== this.oldLabel_) {
          // @ts-ignore: error TS2345: Argument of type '{ __proto__:
          // HTMLElement; decorate(): void; readonly items: any; depth_: number;
          // readonly depth: number; setDepth_(depth: number): void; disabled:
          // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any;
          // }' is not assignable to parameter of type 'EventTarget'.
          dispatchSimpleEvent(this, 'rename', true);
        }
      }
      // @ts-ignore: error TS2339: Property 'oldLabel_' does not exist on type
      // '{ __proto__: HTMLElement; decorate(): void; readonly items: any;
      // depth_: number; readonly depth: number; setDepth_(depth: number): void;
      // disabled: boolean; add(child: TreeItem): void; ... 13 more ...;
      // editing: any; }'.
      delete this.oldLabel_;
    }
  },

  // @ts-ignore: error TS7023: 'editing' implicitly has return type 'any'
  // because it does not have a return type annotation and is referenced
  // directly or indirectly in one of its return expressions.
  get editing() {
    // @ts-ignore: error TS2339: Property 'hasAttribute' does not exist on type
    // '{ __proto__: HTMLElement; decorate(): void; readonly items: any; depth_:
    // number; readonly depth: number; setDepth_(depth: number): void; disabled:
    // boolean; add(child: TreeItem): void; ... 13 more ...; editing: any; }'.
    return this.hasAttribute('editing');
  },
};

/**
 * Helper function that returns the next visible and not disabled tree item.
 * @param {TreeItem} item The tree item.
 * @return {TreeItem} The found item or null.
 */
function getNext(item) {
  // @ts-ignore: error TS2339: Property 'expanded' does not exist on type
  // 'TreeItem'.
  if (item.expanded) {
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'TreeItem'.
    const firstChild = item.items[0];
    if (firstChild) {
      return firstChild;
    }
  }

  return getNextHelper(item);
}

/**
 * Another helper function that returns the next visible and not disabled tree
 * item.
 * @param {TreeItem} item The tree item.
 * @return {TreeItem} The found item or null.
 */
function getNextHelper(item) {
  if (!item) {
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'TreeItem'.
    return null;
  }

  // @ts-ignore: error TS2339: Property 'nextElementSibling' does not exist on
  // type 'TreeItem'.
  const nextSibling = item.nextElementSibling;
  if (nextSibling) {
    if (nextSibling.disabled) {
      // @ts-ignore: error TS2345: Argument of type '(arg0?: Object | undefined)
      // => Element' is not assignable to parameter of type 'new (...arg1:
      // any[]) => TreeItem'.
      return getNextHelper(assertInstanceof(nextSibling, TreeItem));
    }
    // @ts-ignore: error TS2345: Argument of type '(arg0?: Object | undefined)
    // => Element' is not assignable to parameter of type 'new (...arg1: any[])
    // => TreeItem'.
    return assertInstanceof(nextSibling, TreeItem);
  }
  // @ts-ignore: error TS2339: Property 'parentItem' does not exist on type
  // 'TreeItem'.
  return getNextHelper(item.parentItem);
}

/**
 * Helper function that returns the previous visible and not disabled tree item.
 * @param {TreeItem} item The tree item.
 * @return {TreeItem} The found item or null.
 */
function getPrevious(item) {
  // @ts-ignore: error TS2339: Property 'previousElementSibling' does not exist
  // on type 'TreeItem'.
  let previousSibling = item.previousElementSibling;
  while (previousSibling && previousSibling.disabled) {
    previousSibling = previousSibling.previousElementSibling;
  }
  if (previousSibling) {
    // @ts-ignore: error TS2345: Argument of type '(arg0?: Object | undefined)
    // => Element' is not assignable to parameter of type 'new (...arg1: any[])
    // => TreeItem'.
    return getLastHelper(assertInstanceof(previousSibling, TreeItem));
  }
  // @ts-ignore: error TS2339: Property 'parentItem' does not exist on type
  // 'TreeItem'.
  return item.parentItem;
}

/**
 * Helper function that returns the last visible and not disabled tree item in
 * the subtree.
 * @param {TreeItem} item The item to find the last visible item for.
 * @return {TreeItem} The found item or null.
 */
function getLastHelper(item) {
  if (!item) {
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'TreeItem'.
    return null;
  }
  // @ts-ignore: error TS2339: Property 'hasChildren' does not exist on type
  // 'TreeItem'.
  if (item.expanded && item.hasChildren) {
    // @ts-ignore: error TS2339: Property 'items' does not exist on type
    // 'TreeItem'.
    const lastChild = item.items[item.items.length - 1];
    return getLastHelper(lastChild);
  }
  return item;
}
