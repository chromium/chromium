// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';

// Only import types from the DirectoryTree/DirectoryItem/XfTree/XfTreeItem to
// prevent circular imports.
import type {DirectoryItem, DirectoryTree} from '../../foreground/js/ui/directory_tree.js';
import type {XfTree} from '../../widgets/xf_tree.js';
import type {XfTreeItem} from '../../widgets/xf_tree_item.js';
import {isTree, isTreeItem} from '../../widgets/xf_tree_util.js';

import {decorate} from './ui.js';

/**
 * Function to be used as event listener for `mouseenter`, it sets the `title`
 * attribute in the event's element target, when the text content is clipped due
 * to CSS overflow, as in showing `...`.
 *
 * NOTE: This should be used with `mouseenter` because this event triggers less
 * frequent than `mouseover` and `mouseenter` doesn't bubble from the children
 * element to the listener (see mouseenter on MDN for more details).
 */
export function mouseEnterMaybeShowTooltip(event: MouseEvent) {
  const target = event.composedPath()[0] as HTMLElement;
  if (!target) {
    return;
  }
  maybeShowTooltip(target, target.innerText);
}

/**
 * Sets the `title` attribute in the event's element target, when the text
 * content is clipped due to CSS overflow, as in showing `...`.
 */
export function maybeShowTooltip(target: HTMLElement, title: string) {
  if (hasOverflowEllipsis(target)) {
    target.setAttribute('title', title);
  } else {
    target.removeAttribute('title');
  }
}

/**
 * Whether the text content is clipped due to CSS overflow, as in showing `...`.
 */
export function hasOverflowEllipsis(element: HTMLElement) {
  return element.offsetWidth < element.scrollWidth ||
      element.offsetHeight < element.scrollHeight;
}

/** Escapes the symbols: < > & */
export function htmlEscape(str: string): string {
  return str.replace(/[<>&]/g, entity => {
    switch (entity) {
      case '<':
        return '&lt;';
      case '>':
        return '&gt;';
      case '&':
        return '&amp;';
    }
    return entity;
  });
}

/**
 * Returns a string '[Ctrl-][Alt-][Shift-][Meta-]' depending on the event
 * modifiers. Convenient for writing out conditions in keyboard handlers.
 *
 * @param event The keyboard event.
 */
export function getKeyModifiers(event: KeyboardEvent): string {
  return (event.ctrlKey ? 'Ctrl-' : '') + (event.altKey ? 'Alt-' : '') +
      (event.shiftKey ? 'Shift-' : '') + (event.metaKey ? 'Meta-' : '');
}

/**
 * A shortcut function to create a child element with given tag and class.
 *
 * @param parent Parent element.
 * @param className Class name.
 * @param {string=} tag tag, DIV is omitted.
 * @return Newly created element.
 */
export function createChild(
    parent: HTMLElement, className?: string, tag?: string): HTMLElement {
  const child = parent.ownerDocument.createElement(tag || 'div');
  if (className) {
    child.className = className;
  }
  parent.appendChild(child);
  return child;
}

/**
 * Query an element that's known to exist by a selector. We use this instead of
 * just calling querySelector and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param selectors CSS selectors to query the element.
 * @param {(!Document|!DocumentFragment|!Element)=} context An optional
 *     context object for querySelector.
 */
export function queryRequiredElement(
    selectors: string,
    context?: Document|DocumentFragment|Element|HTMLElement): HTMLElement {
  const element = (context || document).querySelector(selectors);
  assertInstanceof(
      element, HTMLElement, 'Missing required element: ' + selectors);
  return element;
}

/**
 * Obtains the element that should exist, decorates it with given type, and
 * returns it.
 * @param query Query for the element.
 * @param type Type used to decorate.
 */
export function queryDecoratedElement<T>(
    query: string, type: {new (...args: any): T}): T {
  const element = queryRequiredElement(query);
  decorate(element, type);
  return element as any as T;
}

/**
 * Returns an array of elements, based on the `selectors`. Exactly one of these
 *  elements is required to exist. The rest will be null.
 * @param selectors A list of CSS selectors to query for elements.
 * @param {(!Document|!DocumentFragment|!Element)=} context An optional
 *     context object for querySelector.
 * @returns A list of query results, with the same indices as the provided
 *     `selectors`. One element will exist, and the rest will be null padding.
 */
export function queryRequiredExactlyOne(
    selectors: string[], context: Document|DocumentFragment|Element = document):
    Array<HTMLElement|null> {
  const elements = selectors.map(
      selector => context.querySelector(selector) as HTMLElement | null);
  assert(
      elements.filter(el => !!el).length === 1,
      'Exactly one of the elements should exist.');
  return elements;
}

/**
 * Creates an instance of UserDomError subtype of DOMError because DOMError is
 * deprecated and its Closure extern is wrong, doesn't have the constructor
 * with 2 arguments. This DOMError looks like a FileError except that it does
 * not have the deprecated FileError.code member.
 *
 * @param  name Error name for the file error.
 * @param {string=} message optional message.
 */
export function createDOMError(name: string, message?: string): DOMError {
  return new UserDomError(name, message);
}

/**
 * Creates a DOMError-like object to be used in place of returning file errors.
 */
class UserDomError extends DOMError {
  private name_: string;
  private message_: string;

  /**
   * @param name Error name for the file error.
   * @param {string=} message Optional message for this error.
   * @suppress {checkTypes} Closure externs for DOMError doesn't have
   * constructor with 1 arg.
   */
  constructor(name: string, message?: string) {
    super(name);

    this.name_ = name;

    this.message_ = message || '';
    Object.freeze(this);
  }

  override get name(): string {
    return this.name_;
  }

  override get message(): string {
    return this.message_;
  }
}

/**
 * A util function to get the correct "top" value when calling
 * <cr-action-menu>'s `showAt` method.
 *
 * @param triggerElement The he element which triggers the menu dropdown.
 * @param marginTop The gap between the trigger element and the menu dialog.
 */
export function getCrActionMenuTop(
    triggerElement: HTMLElement, marginTop: number): number {
  let top = triggerElement.offsetHeight;
  let offsetElement: Element|null = triggerElement;
  // The menu dialog from <cr-action-menu> is "absolute" positioned, we need to
  // start from the trigger element and go upwards to add all offsetTop from all
  // offset parents because each level can have its own offsetTop.
  while (offsetElement instanceof HTMLElement) {
    top += offsetElement.offsetTop;
    offsetElement = offsetElement.offsetParent;
  }
  top += marginTop;
  return top;
}

/**
 * Util functions to check if an HTML element is a tree or tree item, these
 * functions cater both the old tree (cr.ui.Tree/cr.ui.TreeItem) and the new
 * tree (<xf-tree>/<xf-tree-item>).
 *
 * Note: for focused item, the old tree use `selectedItem`, but in the context
 * of the new tree, `selectedItem` means the item being selected, `focusedItem`
 * means the item being focused by keyboard.
 *
 * Use `element: any` here because `DirectoryTree` and `DirectoryItem` are not
 * compatible with Element's type definition, which prevents the type guard. No
 * `instanceof` here to prevent circular imports issue.
 *
 * TODO(b/285977941): Remove the old tree support.
 */
export function isDirectoryTree(element: any): element is DirectoryTree|XfTree {
  return element.typeName === 'directory_tree' || isTree(element);
}
export function isDirectoryTreeItem(element: any): element is DirectoryItem|
    XfTreeItem {
  return element.typeName === 'directory_item' || isTreeItem(element);
}
export function getFocusedTreeItem(tree: any): DirectoryItem|XfTreeItem|null {
  if (tree.typeName === 'directory_tree') {
    return tree.selectedItem;
  }
  if (isTree(tree)) {
    return tree.focusedItem;
  }
  return null;
}
