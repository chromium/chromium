// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert, assertInstanceof} from './assert.js';
import {EventTracker} from './event_tracker.js';
import {hasKeyModifiers, isRTL} from './util.js';
// clang-format on

const ACTIVE_CLASS: string = 'focus-row-active';

/**
 * A class to manage focus between given horizontally arranged elements.
 *
 * Pressing left cycles backward and pressing right cycles forward in item
 * order. Pressing Home goes to the beginning of the list and End goes to the
 * end of the list.
 *
 * If an item in this row is focused, it'll stay active (accessible via tab).
 * If no items in this row are focused, the row can stay active until focus
 * changes to a node inside |this.boundary_|. If |boundary| isn't specified,
 * any focus change deactivates the row.
 */
export class FocusRow {
  root: HTMLElement;
  delegate: FocusRowDelegate|undefined;
  protected eventTracker: EventTracker = new EventTracker();
  private boundary_: Element;

  /**
   * @param root The root of this focus row. Focus classes are
   *     applied to |root| and all added elements must live within |root|.
   * @param boundary Focus events are ignored outside of this element.
   * @param delegate An optional event delegate.
   */
  constructor(root: HTMLElement, boundary: Element|null,
              delegate?: FocusRowDelegate) {
    this.root = root;
    this.boundary_ = boundary || document.documentElement;
    this.delegate = delegate;
  }

  /**
   * Whether it's possible that |element| can be focused.
   */
  static isFocusable(element: Element): boolean {
    if (!element || (element as Element & {disabled?: boolean}).disabled) {
      return false;
    }

    // We don't check that element.tabIndex >= 0 here because inactive rows
    // set a tabIndex of -1.
    let current = element;
    while (true) {
      assertInstanceof(current, Element);

      const style = window.getComputedStyle(current);
      if (style.visibility === 'hidden' || style.display === 'none') {
        return false;
      }

      const parent = current.parentNode;
      if (!parent) {
        return false;
      }

      if (parent === current.ownerDocument ||
          parent instanceof DocumentFragment) {
        return true;
      }

      current = parent as Element;
    }
  }

  /**
   * A focus override is a function that returns an element that should gain
   * focus. The element may not be directly selectable for example the element
   * that can gain focus is in a shadow DOM. Allowing an override via a
   * function leaves the details of how the element is retrieved to the
   * component.
   */
  static getFocusableElement(element: HTMLElement): HTMLElement {
    const withFocusable =
        element as HTMLElement & { getFocusableElement?: () => HTMLElement};
    if (withFocusable.getFocusableElement) {
      return withFocusable.getFocusableElement();
    }
    return element;
  }

  /**
   * Register a new type of focusable element (or add to an existing one).
   *
   * Example: an (X) button might be 'delete' or 'close'.
   *
   * When FocusRow is used within a FocusGrid, these types are used to
   * determine equivalent controls when Up/Down are pressed to change rows.
   *
   * Another example: mutually exclusive controls that hide each other on
   * activation (i.e. Play/Pause) could use the same type (i.e. 'play-pause')
   * to indicate they're equivalent.
   *
   * @param type The type of element to track focus of.
   * @param selectorOrElement The selector of the element
   *    from this row's root, or the element itself.
   * @return Whether a new item was added.
   */
  addItem(type: string, selectorOrElement: string|HTMLElement): boolean {
    assert(type);

    let element;
    if (typeof selectorOrElement === 'string') {
      element = this.root.querySelector<HTMLElement>(selectorOrElement);
    } else {
      element = selectorOrElement;
    }
    if (!element) {
      return false;
    }

    element.setAttribute('focus-type', type);
    element.tabIndex = this.isActive() ? 0 : -1;

    this.eventTracker.add(element, 'blur', this.onBlur_.bind(this));
    this.eventTracker.add(element, 'focus', this.onFocus_.bind(this));
    this.eventTracker.add(element, 'keydown', this.onKeydown_.bind(this));
    this.eventTracker.add(element, 'mousedown', this.onMousedown_.bind(this));
    return true;
  }

  /** Dereferences nodes and removes event handlers. */
  destroy() {
    this.eventTracker.removeAll();
  }

  /**
   * @param sampleElement An element for to find an equivalent
   *     for.
   * @return An equivalent element to focus for
   *     |sampleElement|.
   */
  protected getCustomEquivalent(_sampleElement: HTMLElement): HTMLElement {
    const focusable = this.getFirstFocusable();
    assert(focusable);
    return focusable;
  }

  /**
   * @return All registered elements (regardless of focusability).
   */
  getElements(): HTMLElement[] {
    return Array.from(this.root.querySelectorAll<HTMLElement>('[focus-type]'))
        .map(FocusRow.getFocusableElement);
  }

  /**
   * Find the element that best matches |sampleElement|.
   * @param sampleElement An element from a row of the same
   *     type which previously held focus.
   * @return The element that best matches sampleElement.
   */
  getEquivalentElement(sampleElement: HTMLElement): HTMLElement {
    if (this.getFocusableElements().indexOf(sampleElement) >= 0) {
      return sampleElement;
    }

    const sampleFocusType = this.getTypeForElement(sampleElement);
    if (sampleFocusType) {
      const sameType = this.getFirstFocusable(sampleFocusType);
      if (sameType) {
        return sameType;
      }
    }

    return this.getCustomEquivalent(sampleElement);
  }

  /**
   * @param type An optional type to search for.
   * @return The first focusable element with |type|.
   */
  getFirstFocusable(type?: string): HTMLElement|null {
    const element = this.getFocusableElements().find(
        el => !type || el.getAttribute('focus-type') === type);
    return element || null;
  }

  /** @return Registered, focusable elements. */
  getFocusableElements(): HTMLElement[] {
    return this.getElements().filter(FocusRow.isFocusable);
  }

  /**
   * @param element An element to determine a focus type for.
   * @return The focus type for |element| or '' if none.
   */
  getTypeForElement(element: Element): string {
    return element.getAttribute('focus-type') || '';
  }

  /** @return Whether this row is currently active. */
  isActive(): boolean {
    return this.root.classList.contains(ACTIVE_CLASS);
  }

  /**
   * Enables/disables the tabIndex of the focusable elements in the FocusRow.
   * tabIndex can be set properly.
   * @param active True if tab is allowed for this row.
   */
  makeActive(active: boolean) {
    if (active === this.isActive()) {
      return;
    }

    this.getElements().forEach(function(element) {
      element.tabIndex = active ? 0 : -1;
    });

    this.root.classList.toggle(ACTIVE_CLASS, active);
  }

  private onBlur_(e: FocusEvent) {
    if (!this.boundary_.contains(e.relatedTarget as Element)) {
      return;
    }

    const currentTarget = e.currentTarget as HTMLElement;
    if (this.getFocusableElements().indexOf(currentTarget) >= 0) {
      this.makeActive(false);
    }
  }

  private onFocus_(e: Event) {
    if (this.delegate) {
      this.delegate.onFocus(this, e);
    }
  }

  private onMousedown_(e: MouseEvent) {
    // Only accept left mouse clicks.
    if (e.button) {
      return;
    }

    // Allow the element under the mouse cursor to be focusable.
    const target = e.currentTarget as HTMLElement & {disabled?: boolean};
    if (!target.disabled) {
      target.tabIndex = 0;
    }
  }

  private onKeydown_(e: KeyboardEvent) {
    const elements = this.getFocusableElements();
    const currentElement = FocusRow.getFocusableElement(
        e.currentTarget as HTMLElement);
    const elementIndex = elements.indexOf(currentElement);
    assert(elementIndex >= 0);

    if (this.delegate && this.delegate.onKeydown(this, e)) {
      return;
    }

    const isShiftTab = !e.altKey && !e.ctrlKey && !e.metaKey && e.shiftKey &&
        e.key === 'Tab';

    if (hasKeyModifiers(e) && !isShiftTab) {
      return;
    }

    let index = -1;
    let shouldStopPropagation = true;

    if (isShiftTab) {
      // This always moves back one element, even in RTL.
      index = elementIndex - 1;
      if (index < 0) {
        // Bubble up to focus on the previous element outside the row.
        return;
      }
    } else if (e.key === 'ArrowLeft') {
      index = elementIndex + (isRTL() ? 1 : -1);
    } else if (e.key === 'ArrowRight') {
      index = elementIndex + (isRTL() ? -1 : 1);
    } else if (e.key === 'Home') {
      index = 0;
    } else if (e.key === 'End') {
      index = elements.length - 1;
    } else {
      shouldStopPropagation = false;
    }

    const elementToFocus = elements[index];
    if (elementToFocus) {
      this.getEquivalentElement(elementToFocus).focus();
      e.preventDefault();
    }
    if (shouldStopPropagation) {
      e.stopPropagation();
    }
  }
}

export interface FocusRowDelegate {
  /**
   * Called when a key is pressed while on a FocusRow's item. If true is
   * returned, further processing is skipped.
   * @param row The row that detected a keydown.
   * @return Whether the event was handled.
   */
  onKeydown(row: FocusRow, e: KeyboardEvent): boolean;

  onFocus(row: FocusRow, e: Event): void;

  /**
   * @param sampleElement An element to find an equivalent for.
   * @return An equivalent element to focus, or null to use the
   *     default FocusRow element.
   */
  getCustomEquivalent(sampleElement: HTMLElement): HTMLElement|null;
}

export class VirtualFocusRow extends FocusRow {
  constructor(root: HTMLElement, delegate: FocusRowDelegate) {
    super(root, /* boundary */ null, delegate);
  }

  override getCustomEquivalent(sampleElement: HTMLElement) {
    const equivalent =
        this.delegate ? this.delegate.getCustomEquivalent(sampleElement) : null;
    return equivalent || super.getCustomEquivalent(sampleElement);
  }
}
