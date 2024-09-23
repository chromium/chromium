// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {FocusRow, VirtualFocusRow} from '//resources/js/focus_row.js';

import {FocusRowMixinDelegate} from './focus_row_mixin_delegate.js';
// clang-format on

/**
 * Any element that is being used as an iron-list or infinite-list row item can
 * extend this mixin, which encapsulates focus controls of mouse and keyboards.
 * To use this mixin:
 *    - The parent element should pass a "last-focused" attribute double-bound
 *      to the row items, to track the last-focused element across rows, and
 *      a "list-blurred" attribute double-bound to the row items, to track
 *      whether the list of row items has been blurred.
 *    - There must be a container in the extending element with the
 *      [focus-row-container] attribute that contains all focusable controls.
 *    - On each of the focusable controls, there must be a [focus-row-control]
 *      attribute, and a [focus-type=] attribute unique for each control.
 *
 */
type Constructor<T> = new (...args: any[]) => T;

export const FocusRowMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<FocusRowMixinLitInterface> => {
  class FocusRowMixinLit extends superClass implements
      FocusRowMixinLitInterface {
    static get properties() {
      return {
        row_: {type: Object},
        mouseFocused_: {type: Boolean},

        // Will be updated when |index| is set, unless specified elsewhere.
        id: {
          type: String,
          reflect: true,
        },

        isFocused: {
          type: Boolean,
          notify: true,
        },

        focusRowIndex: {type: Number},

        lastFocused: {
          type: Object,
          notify: true,
        },

        listTabIndex: {type: Number},

        listBlurred: {
          type: Boolean,
          notify: true,
        },
      };
    }

    private row_: VirtualFocusRow|null = null;
    private mouseFocused_: boolean = false;

    // For notifying when the row is in focus.
    isFocused: boolean = false;

    // Should be bound to the index of the item from the iron-list or
    // infinite-list.
    focusRowIndex?: number;

    lastFocused: HTMLElement|null = null;

    /**
     * This is different from tabIndex, since the template only does a
     * one-way binding on both attributes, and the mixin makes use of this.
     * For example, when a control within a row is focused, it will have
     * tabIndex = -1 and listTabIndex = 0.
     */
    listTabIndex?: number;
    listBlurred: boolean = false;

    private firstControl_: HTMLElement|null = null;
    private controlObservers_: MutationObserver[] = [];
    private subtreeObserver_: MutationObserver|null = null;
    private boundOnFirstControlKeydown_: ((e: Event) => void)|null = null;

    override connectedCallback() {
      super.connectedCallback();
      this.classList.add('no-outline');
      this.boundOnFirstControlKeydown_ = this.onFirstControlKeydown_.bind(this);

      this.updateComplete.then(() => {
        const rowContainer = this.shadowRoot!.querySelector<HTMLElement>(
            '[focus-row-container]');
        assert(rowContainer);
        this.row_ =
            new VirtualFocusRow(rowContainer, new FocusRowMixinDelegate(this));
        this.addItems_();

        // Adding listeners asynchronously to reduce blocking time, since
        // this behavior will be used by items in potentially long lists.
        this.addEventListener('focus', this.onFocus_);
        this.subtreeObserver_ = new MutationObserver(() => this.addItems_());
        this.subtreeObserver_.observe(
            this.shadowRoot!, {childList: true, subtree: true});
        this.addEventListener('mousedown', this.onMouseDown_);
        this.addEventListener('blur', this.onBlur_);
      });
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      this.removeEventListener('focus', this.onFocus_);
      if (this.subtreeObserver_) {
        this.subtreeObserver_.disconnect();
        this.subtreeObserver_ = null;
      }
      this.removeEventListener('mousedown', this.onMouseDown_);
      this.removeEventListener('blur', this.onBlur_);
      this.removeObservers_();
      if (this.firstControl_ && this.boundOnFirstControlKeydown_) {
        this.firstControl_.removeEventListener(
            'keydown', this.boundOnFirstControlKeydown_);
        this.boundOnFirstControlKeydown_ = null;
      }
      if (this.row_) {
        this.row_.destroy();
      }
    }

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      if (changedProperties.has('focusRowIndex') &&
          this.focusRowIndex !== undefined) {
        // focusRowIndex is 0-based where aria-rowindex is 1-based.
        this.setAttribute('aria-rowindex', (this.focusRowIndex + 1).toString());

        // Only set ID if it matches what was previously set. This prevents
        // overriding the ID value if it's set elsewhere.
        const oldIndex = changedProperties.get('focusRowIndex');
        if (this.id === this.computeId_(oldIndex)) {
          this.id = this.computeId_(this.focusRowIndex) || '';
        }
      }
    }

    override updated(changedProperties: PropertyValues<this>) {
      super.updated(changedProperties);

      if (changedProperties.has('listTabIndex')) {
        this.listTabIndexChanged_();
      }
    }

    /**
     * Returns an ID based on the index that was passed in.
     */
    private computeId_(index: number|undefined): string|undefined {
      return index !== undefined ? `frb${index}` : undefined;
    }

    getFocusRow(): FocusRow {
      assert(this.row_);
      return this.row_;
    }

    private updateFirstControl_() {
      assert(this.row_);
      const newFirstControl = this.row_.getFirstFocusable();
      if (newFirstControl === this.firstControl_) {
        return;
      }

      if (this.firstControl_) {
        this.firstControl_.removeEventListener(
            'keydown', this.boundOnFirstControlKeydown_!);
      }
      this.firstControl_ = newFirstControl;
      if (this.firstControl_) {
        this.firstControl_.addEventListener(
            'keydown', this.boundOnFirstControlKeydown_!);
      }
    }

    private removeObservers_() {
      if (this.controlObservers_.length > 0) {
        this.controlObservers_.forEach(observer => {
          observer.disconnect();
        });
      }
      this.controlObservers_ = [];
    }

    private addItems_() {
      this.listTabIndexChanged_();
      if (this.row_) {
        this.removeObservers_();
        this.row_.destroy();

        const controls = this.shadowRoot!.querySelectorAll<HTMLElement>(
            '[focus-row-control]');

        controls.forEach(control => {
          assert(control);
          assert(this.row_);
          this.row_.addItem(
              control.getAttribute('focus-type')!,
              FocusRow.getFocusableElement(control));
          this.addMutationObservers_(control);
        });
        this.updateFirstControl_();
      }
    }

    private createObserver_(): MutationObserver {
      return new MutationObserver(mutations => {
        const mutation = mutations[0]!;
        if (mutation.attributeName === 'style' && mutation.oldValue) {
          const newStyle = window.getComputedStyle(mutation.target as Element);
          const oldDisplayValue = mutation.oldValue.match(/^display:(.*)(?=;)/);
          const oldVisibilityValue =
              mutation.oldValue.match(/^visibility:(.*)(?=;)/);
          // Return early if display and visibility have not changed.
          if (oldDisplayValue &&
              newStyle.display === oldDisplayValue[1]!.trim() &&
              oldVisibilityValue &&
              newStyle.visibility === oldVisibilityValue[1]!.trim()) {
            return;
          }
        }
        this.updateFirstControl_();
      });
    }

    /**
     * The first focusable control changes if hidden, disabled, or
     * style.display changes for the control or any of its ancestors. Add
     * mutation observers to watch for these changes in order to ensure the
     * first control keydown listener is always on the correct element.
     */
    private addMutationObservers_(control: Element) {
      let current: Node|null = control;
      while (current && current !== this.shadowRoot) {
        const currentObserver = this.createObserver_();
        currentObserver.observe(current as Element, {
          attributes: true,
          attributeFilter: ['hidden', 'disabled', 'style'],
          attributeOldValue: true,
        });
        this.controlObservers_.push(currentObserver);
        current = current.parentNode;
      }
    }

    /**
     * This function gets called when the row itself receives the focus
     * event.
     */
    private onFocus_(e: Event) {
      if (this.mouseFocused_) {
        this.mouseFocused_ = false;  // Consume and reset flag.
        return;
      }

      // If focus is being restored from outside the item and the event is
      // fired by the list item itself, focus the first control so that the
      // user can tab through all the controls. When the user shift-tabs
      // back to the row, or focus is restored to the row from a dropdown on
      // the last item, the last child item will be focused before the row
      // itself. Since this is the desired behavior, do not shift focus to
      // the first item in these cases.
      const restoreFocusToFirst =
          this.listBlurred && e.composedPath()[0] === this;

      if (this.lastFocused && !restoreFocusToFirst) {
        assert(this.row_);
        focusWithoutInk(this.row_.getEquivalentElement(this.lastFocused));
      } else {
        assert(this.firstControl_);
        const firstFocusable = this.firstControl_;
        focusWithoutInk(firstFocusable);
      }
      this.listBlurred = false;
      this.isFocused = true;
    }

    private onFirstControlKeydown_(e: Event) {
      const keyEvent = e as KeyboardEvent;
      if (keyEvent.shiftKey && keyEvent.key === 'Tab') {
        this.focus();
      }
    }

    private listTabIndexChanged_() {
      if (this.row_) {
        this.row_.makeActive(this.listTabIndex === 0);
      }

      // If a new row is being focused, reset listBlurred. This means an
      // item has been removed and iron-list is about to focus the next
      // item.
      if (this.listTabIndex === 0) {
        this.listBlurred = false;
      }
    }

    private onMouseDown_() {
      this.mouseFocused_ = true;  // Set flag to not do any control-focusing.
    }

    private onBlur_(e: FocusEvent) {
      // Reset focused flags since it's not active anymore.
      this.mouseFocused_ = false;
      this.isFocused = false;

      const node = e.relatedTarget ? e.relatedTarget as Node : null;
      if (!this.parentNode!.contains(node)) {
        this.listBlurred = true;
      }
    }
  }

  return FocusRowMixinLit;
};

export interface FocusRowMixinLitInterface {
  id: string;
  isFocused: boolean;
  focusRowIndex?: number;
  lastFocused: HTMLElement|null;
  listTabIndex?: number;
  listBlurred: boolean;
  overrideCustomEquivalent?: boolean;

  getCustomEquivalent?(el: HTMLElement): HTMLElement|null;

  getFocusRow(): FocusRow;
}
