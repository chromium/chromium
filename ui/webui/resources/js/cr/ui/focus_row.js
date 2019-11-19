// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js'
// #import {EventTracker} from 'chrome://resources/js/event_tracker.m.js'
// #import {hasKeyModifiers, isRTL} from 'chrome://resources/js/util.m.js'
// clang-format on

cr.define('cr.ui', function() {
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
  /* #export */ class FocusRow {
    /**
     * @param {!Element} root The root of this focus row. Focus classes are
     *     applied to |root| and all added elements must live within |root|.
     * @param {?Element} boundary Focus events are ignored outside of this
     *     element.
     * @param {cr.ui.FocusRowDelegate=} delegate An optional event
     *     delegate.
     */
    constructor(root, boundary, delegate) {
      /** @type {!Element} */
      this.root = root;

      /** @private {!Element} */
      this.boundary_ = boundary || document.documentElement;

      /** @type {cr.ui.FocusRowDelegate|undefined} */
      this.delegate = delegate;

      /** @protected {!EventTracker} */
      this.eventTracker = new EventTracker;
    }

    /**
     * Whether it's possible that |element| can be focused.
     * @param {Element} element
     * @return {boolean} Whether the item is focusable.
     */
    static isFocusable(element) {
      if (!element || element.disabled) {
        return false;
      }

      // We don't check that element.tabIndex >= 0 here because inactive rows
      // set a tabIndex of -1.
      let current = element;
      while (true) {
        assertInstanceof(current, Element);

        const style = window.getComputedStyle(current);
        if (style.visibility == 'hidden' || style.display == 'none') {
          return false;
        }

        const parent = current.parentNode;
        if (!parent) {
          return false;
        }

        if (parent == current.ownerDocument ||
            parent instanceof DocumentFragment) {
          return true;
        }

        current = /** @type {Element} */ (parent);
      }
    }

    /**
     * A focus override is a function that returns an element that should gain
     * focus. The element may not be directly selectable for example the element
     * that can gain focus is in a shadow DOM. Allowing an override via a
     * function leaves the details of how the element is retrieved to the
     * component.
     * @param {!Element} element
     * @return {!Element}
     */
    static getFocusableElement(element) {
      if (element.getFocusableElement) {
        return element.getFocusableElement();
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
     * @param {string} type The type of element to track focus of.
     * @param {string|HTMLElement} selectorOrElement The selector of the element
     *    from this row's root, or the element itself.
     * @return {boolean} Whether a new item was added.
     */
    addItem(type, selectorOrElement) {
      assert(type);

      let element;
      if (typeof selectorOrElement == 'string') {
        element = this.root.querySelector(selectorOrElement);
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
     * @param {!Element} sampleElement An element for to find an equivalent for.
     * @return {!Element} An equivalent element to focus for |sampleElement|.
     * @protected
     */
    getCustomEquivalent(sampleElement) {
      return /** @type {!Element} */ (assert(this.getFirstFocusable()));
    }

    /**
     * @return {!Array<!Element>} All registered elements (regardless of
     *     focusability).
     */
    getElements() {
      return Array.from(this.root.querySelectorAll('[focus-type]'))
          .map(cr.ui.FocusRow.getFocusableElement);
    }

    /**
     * Find the element that best matches |sampleElement|.
     * @param {!Element} sampleElement An element from a row of the same type
     *     which previously held focus.
     * @return {!Element} The element that best matches sampleElement.
     */
    getEquivalentElement(sampleElement) {
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
     * @param {string=} opt_type An optional type to search for.
     * @return {?Element} The first focusable element with |type|.
     */
    getFirstFocusable(opt_type) {
      const element = this.getFocusableElements().find(
          el => !opt_type || el.getAttribute('focus-type') == opt_type);
      return element || null;
    }

    /** @return {!Array<!Element>} Registered, focusable elements. */
    getFocusableElements() {
      return this.getElements().filter(cr.ui.FocusRow.isFocusable);
    }

    /**
     * @param {!Element} element An element to determine a focus type for.
     * @return {string} The focus type for |element| or '' if none.
     */
    getTypeForElement(element) {
      return element.getAttribute('focus-type') || '';
    }

    /** @return {boolean} Whether this row is currently active. */
    isActive() {
      return this.root.classList.contains(FocusRow.ACTIVE_CLASS);
    }

    /**
     * Enables/disables the tabIndex of the focusable elements in the FocusRow.
     * tabIndex can be set properly.
     * @param {boolean} active True if tab is allowed for this row.
     */
    makeActive(active) {
      if (active == this.isActive()) {
        return;
      }

      this.getElements().forEach(function(element) {
        element.tabIndex = active ? 0 : -1;
      });

      this.root.classList.toggle(FocusRow.ACTIVE_CLASS, active);
    }

    /**
     * @param {!Event} e
     * @private
     */
    onBlur_(e) {
      if (!this.boundary_.contains(/** @type {Element} */ (e.relatedTarget))) {
        return;
      }

      const currentTarget = /** @type {!Element} */ (e.currentTarget);
      if (this.getFocusableElements().indexOf(currentTarget) >= 0) {
        this.makeActive(false);
      }
    }

    /**
     * @param {!Event} e
     * @private
     */
    onFocus_(e) {
      if (this.delegate) {
        this.delegate.onFocus(this, e);
      }
    }

    /**
     * @param {!Event} e A mousedown event.
     * @private
     */
    onMousedown_(e) {
      // Only accept left mouse clicks.
      if (e.button) {
        return;
      }

      // Allow the element under the mouse cursor to be focusable.
      if (!e.currentTarget.disabled) {
        e.currentTarget.tabIndex = 0;
      }
    }

    /**
     * @param {!Event} e The keydown event.
     * @private
     */
    onKeydown_(e) {
      const elements = this.getFocusableElements();
      const currentElement = cr.ui.FocusRow.getFocusableElement(
          /** @type {!Element} */ (e.currentTarget));
      const elementIndex = elements.indexOf(currentElement);
      assert(elementIndex >= 0);

      if (this.delegate && this.delegate.onKeydown(this, e)) {
        return;
      }

      if (hasKeyModifiers(e)) {
        return;
      }

      let index = -1;

      if (e.key == 'ArrowLeft') {
        index = elementIndex + (isRTL() ? 1 : -1);
      } else if (e.key == 'ArrowRight') {
        index = elementIndex + (isRTL() ? -1 : 1);
      } else if (e.key == 'Home') {
        index = 0;
      } else if (e.key == 'End') {
        index = elements.length - 1;
      }

      const elementToFocus = elements[index];
      if (elementToFocus) {
        this.getEquivalentElement(elementToFocus).focus();
        e.preventDefault();
      }
    }
  }

  /** @const {string} */
  FocusRow.ACTIVE_CLASS = 'focus-row-active';


  /** @interface */
  /* #export */ class FocusRowDelegate {
    /**
     * Called when a key is pressed while on a FocusRow's item. If true is
     * returned, further processing is skipped.
     * @param {!cr.ui.FocusRow} row The row that detected a keydown.
     * @param {!Event} e
     * @return {boolean} Whether the event was handled.
     */
    onKeydown(row, e) {}

    /**
     * @param {!cr.ui.FocusRow} row
     * @param {!Event} e
     */
    onFocus(row, e) {}

    /**
     * @param {!Element} sampleElement An element to find an equivalent for.
     * @return {?Element} An equivalent element to focus, or null to use the
     *     default FocusRow element.
     */
    getCustomEquivalent(sampleElement) {}
  }

  // #cr_define_end
  return {
    FocusRow,
    FocusRowDelegate,
  };
});
