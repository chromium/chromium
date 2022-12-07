// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert_ts.js';
import {decorate} from './ui.js';
import type {CSSResult} from '../../widgets/xf_base.js';

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
    selectors: string, context?: Document|DocumentFragment|Element): Element {
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
 * Add prefix selector for the CSS literal.
 * To support both Legacy and Refresh23 styles in the same component, we
 * have 2 style groups defined in each component, for all legacy/refresh23
 * specific styles, we need to prefix all rules to have
 * `[theme=legacy]` and `[theme=refresh23]` so they won't conflict
 * with each other.
 *
 * For example:
 * original style -> p { color: red; }
 * prefix with Legacy -> :host-context([theme="legacy"]) p { color: red }
 * prefix with Refresh23 -> :host-context([theme="refresh23"]) p { color: red }
 */
export function addCSSPrefixSelector(
    css: CSSResult, prefixSelector: string): CSSStyleSheet {
  const prefixedCSS = new CSSStyleSheet();
  const cssRules = css.styleSheet?.cssRules || [];
  for (let i = 0; i < cssRules.length; i++) {
    const cssText = cssRules[i]?.cssText;
    if (cssText) {
      // If the existing selector is `:host` or `:host-context`, there should
      // be no space after the newly added `:host-context`.
      const noSpace = cssText.startsWith(':host');
      prefixedCSS.insertRule(
          `:host-context(${prefixSelector})${noSpace ? '' : ' '}${cssText}`, i);
    }
  }
  return prefixedCSS;
}
