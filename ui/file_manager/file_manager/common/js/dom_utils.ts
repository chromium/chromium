// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';

// Only import types from the XfTree/XfTreeItem to
// prevent circular imports.
import type {XfTreeItem} from '../../widgets/xf_tree_item.js';
import {isTreeItem, isXfTree} from '../../widgets/xf_tree_util.js';

import {crInjectTypeAndInit, type DecoratableElement} from './cr_ui.js';

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
export function queryDecoratedElement<T extends DecoratableElement>(
    query: string, type: {new (...args: any): T}): T {
  const element = queryRequiredElement(query);
  crInjectTypeAndInit(element, type);
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
  const elements =
      selectors.map(selector => context.querySelector<HTMLElement>(selector));
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

export function getFocusedTreeItem(treeOrTreeItem: HTMLElement|Element|
                                   EventTarget|null): XfTreeItem|null {
  if (!treeOrTreeItem) {
    return null;
  }
  if (isXfTree(treeOrTreeItem)) {
    return treeOrTreeItem.focusedItem;
  }
  if (isTreeItem(treeOrTreeItem) && treeOrTreeItem.tree) {
    return treeOrTreeItem.tree.focusedItem;
  }
  return null;
}
