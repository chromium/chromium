// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent, getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {define as crUiDefine, limitInputWidth} from '../../../common/js/ui.js';
// clang-format on

/**
 * The number of pixels to indent per level.
 * @type {number}
 * @const
 */
const INDENT = 20;

/**
 * A custom rowElement depth (indent) style handler where undefined uses the
 * default depth INDENT styling, see TreeItem.setDepth_().
 *
 * @type {function(!TreeItem,number)|undefined}
 */
let customRowElementDepthStyleHandler = undefined;

/**
 * Returns the computed style for an element.
 * @param {!Element} el The element to get the computed style for.
 * @return {!CSSStyleDeclaration} The computed style.
 */
function getComputedStyle(el) {
  return assert(el.ownerDocument.defaultView.getComputedStyle(el));
}

/**
 * Helper function that finds the first ancestor tree item.
 * @param {Node} node The node to start searching from.
 * @return {TreeItem} The found tree item or null if not found.
 */
function findTreeItem(node) {
  while (node && !(node instanceof TreeItem)) {
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
export const Tree = crUiDefine('tree');

Tree.prototype = {
  __proto__: HTMLElement.prototype,

  /**
   * Initializes the element.
   */
  decorate() {
    // Make list focusable
    if (!this.hasAttribute('tabindex')) {
      this.tabIndex = 0;
    }

    this.addEventListener('click', this.handleClick);
    this.addEventListener('mousedown', this.handleMouseDown);
    this.addEventListener('dblclick', this.handleDblClick);
    this.addEventListener('keydown', this.handleKeyDown);

    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'tree');
    }
  },

  /**
   * Returns the tree item rowElement style handler.
   *
   * @return {function(!TreeItem,number)|undefined}
   */
  get rowElementDepthStyleHandler() {
    return customRowElementDepthStyleHandler;
  },

  /**
   * Sets a tree item rowElement style handler, which allows Tree users to
   * customize the depth (indent) style of tree item rowElements.
   *
   * @param {function(!TreeItem,number)|undefined} handler
   */
  set rowElementDepthStyleHandler(handler) {
    customRowElementDepthStyleHandler = handler;
  },

  /**
   * Returns the tree item that are children of this tree.
   */
  get items() {
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
    this.insertBefore(treeItem, this.children[index]);
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
    if (treeItem && !treeItem.disabled) {
      treeItem.handleClick(e);
    }
  },

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
    if (treeItem && !treeItem.disabled) {
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
    if (e.ctrlKey) {
      return;
    }

    const item = this.selectedItem;
    if (!item) {
      return;
    }

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
        if (e.altKey) {
          break;
        }

        if (e.key === 'ArrowLeft' && !rtl || e.key === 'ArrowRight' && rtl) {
          if (item.expanded) {
            item.expanded = false;
          } else {
            itemToSelect = findTreeItem(item.parentNode);
          }
        } else {
          if (!item.expanded) {
            item.expanded = true;
          } else {
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
    return this.selectedItem_ || null;
  },
  set selectedItem(item) {
    const oldSelectedItem = this.selectedItem_;
    if (oldSelectedItem !== item) {
      // Set the selectedItem_ before deselecting the old item since we only
      // want one change when moving between items.
      this.selectedItem_ = item;

      if (oldSelectedItem) {
        oldSelectedItem.selected = false;
      }

      if (item) {
        item.selected = true;
        if (item.id) {
          this.setAttribute('aria-activedescendant', item.id);
        }
      } else {
        this.removeAttribute('aria-activedescendant');
      }
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
      return this.selectedItem.rowElement.getBoundingClientRect();
    }
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
export const TreeItem = crUiDefine(function() {
  const treeItem = treeItemProto.cloneNode(true);
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
    this.setAttribute('aria-labelledby', labelId);
  },

  /**
   * The tree items children.
   */
  get items() {
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
    return this.hasAttribute('disabled');
  },
  set disabled(b) {
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
    this.lastElementChild.insertBefore(child, this.items[index]);
    if (this.items.length === 1) {
      this.hasChildren = true;
    }
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
    const selectedItem = tree.selectedItem;
    if (selectedItem && child.contains(selectedItem)) {
      this.selected = true;
    }

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
      t = t.parentItem;
    }
    return t;
  },

  /**
   * Whether the tree item is expanded or not.
   * @type {boolean}
   */
  get expanded() {
    return this.hasAttribute('expanded');
  },
  set expanded(b) {
    if (this.expanded === b) {
      return;
    }

    const treeChildren = this.lastElementChild;

    if (b) {
      if (this.mayHaveChildren_) {
        this.setAttribute('expanded', '');
        this.setAttribute('aria-expanded', 'true');
        treeChildren.setAttribute('expanded', '');
        dispatchSimpleEvent(this, 'expand', true);
        this.scrollIntoViewIfNeeded(false);
      }
    } else {
      const tree = this.tree;
      if (tree && !this.selected) {
        const oldSelected = tree.selectedItem;
        if (oldSelected && this.contains(oldSelected)) {
          this.selected = true;
        }
      }
      this.removeAttribute('expanded');
      if (this.mayHaveChildren_) {
        this.setAttribute('aria-expanded', 'false');
      } else {
        this.removeAttribute('aria-expanded');
      }
      treeChildren.removeAttribute('expanded');
      dispatchSimpleEvent(this, 'collapse', true);
    }
  },

  /**
   * Expands all parent items.
   */
  reveal() {
    let pi = this.parentItem;
    while (pi && !(pi instanceof Tree)) {
      pi.expanded = true;
      pi = pi.parentItem;
    }
  },

  /**
   * The element representing the row that gets highlighted.
   * @type {!HTMLElement}
   */
  get rowElement() {
    return this.firstElementChild;
  },

  /**
   * The element containing the label text.
   * @type {!HTMLElement}
   */
  get labelElement() {
    return this.rowElement.lastElementChild;
  },

  /**
   * The label text.
   * @type {string}
   */
  get label() {
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
    return this.hasAttribute('selected');
  },
  set selected(b) {
    if (this.selected === b) {
      return;
    }
    const rowItem = this.rowElement;
    const tree = this.tree;
    if (b) {
      this.setAttribute('selected', '');
      rowItem.setAttribute('selected', '');
      this.reveal();
      this.labelElement.scrollIntoViewIfNeeded(false);
      if (tree) {
        tree.selectedItem = this;
      }
    } else {
      this.removeAttribute('selected');
      rowItem.removeAttribute('selected');
      if (tree && tree.selectedItem === this) {
        tree.selectedItem = null;
      }
    }
  },

  /**
   * Whether the tree item has children.
   * @type {boolean}
   */
  get mayHaveChildren_() {
    return this.hasAttribute('may-have-children');
  },
  set mayHaveChildren_(b) {
    const rowItem = this.rowElement;
    if (b) {
      this.setAttribute('may-have-children', '');
      rowItem.setAttribute('may-have-children', '');
    } else {
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
    this.setAttribute('has-children', b);
    rowItem.setAttribute('has-children', b);
    if (b) {
      this.mayHaveChildren_ = true;
      this.setAttribute('aria-expanded', this.expanded);
    }
  },

  /**
   * Called when the user clicks on a tree item. This is forwarded from the
   * Tree.
   * @param {Event} e The click event.
   */
  handleClick(e) {
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
    let input;

    // Handles enter and escape which trigger reset and commit respectively.
    function handleKeydown(e) {
      // Make sure that the tree does not handle the key.
      e.stopPropagation();

      // Calling tree.focus blurs the input which will make the tree item
      // non editable.
      switch (e.key) {
        case 'Escape':
          input.value = text;
        // fall through
        case 'Enter':
          self.tree.focus();
      }
    }

    function stopPropagation(e) {
      e.stopPropagation();
    }

    if (editing) {
      this.selected = true;
      this.setAttribute('editing', '');
      this.draggable = false;

      // We create an input[type=text] and copy over the label value. When
      // the input loses focus we set editing to false again.
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
        input.addEventListener(type, stopPropagation);
      });

      // Wait for the input element to recieve focus before sizing it.
      const rowElement = this.rowElement;
      const onFocus = function() {
        input.removeEventListener('focus', onFocus);
        // 20 = the padding and border of the tree-row
        limitInputWidth(input, rowElement, 100);
      };
      input.addEventListener('focus', onFocus);
      input.focus();
      input.select();

      this.oldLabel_ = text;
    } else {
      this.removeAttribute('editing');
      this.draggable = true;
      input = labelEl.firstChild;
      const value = input.value;
      if (/^\s*$/.test(value)) {
        labelEl.textContent = this.oldLabel_;
      } else {
        labelEl.textContent = value;
        if (value !== this.oldLabel_) {
          dispatchSimpleEvent(this, 'rename', true);
        }
      }
      delete this.oldLabel_;
    }
  },

  get editing() {
    return this.hasAttribute('editing');
  },
};

/**
 * Helper function that returns the next visible and not disabled tree item.
 * @param {TreeItem} item The tree item.
 * @return {TreeItem} The found item or null.
 */
function getNext(item) {
  if (item.expanded) {
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
    return null;
  }

  const nextSibling = item.nextElementSibling;
  if (nextSibling) {
    if (nextSibling.disabled) {
      return getNextHelper(assertInstanceof(nextSibling, TreeItem));
    }
    return assertInstanceof(nextSibling, TreeItem);
  }
  return getNextHelper(item.parentItem);
}

/**
 * Helper function that returns the previous visible and not disabled tree item.
 * @param {TreeItem} item The tree item.
 * @return {TreeItem} The found item or null.
 */
function getPrevious(item) {
  let previousSibling = item.previousElementSibling;
  while (previousSibling && previousSibling.disabled) {
    previousSibling = previousSibling.previousElementSibling;
  }
  if (previousSibling) {
    return getLastHelper(assertInstanceof(previousSibling, TreeItem));
  }
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
    return null;
  }
  if (item.expanded && item.hasChildren) {
    const lastChild = item.items[item.items.length - 1];
    return getLastHelper(lastChild);
  }
  return item;
}
