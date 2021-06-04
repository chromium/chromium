// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-tabs' is a control used for selecting different sections or
 * tabs. cr-tabs was created to replace paper-tabs and paper-tab. cr-tabs
 * displays the name of each tab provided by |tabs|. A 'selected-changed' event
 * is fired any time |selected| is changed.
 *
 * cr-tabs takes its #selectionBar animation from paper-tabs.
 *
 * Keyboard behavior
 *   - Home, End, ArrowLeft and ArrowRight changes the tab selection
 *
 * Known limitations
 *   - no "disabled" state for the cr-tabs as a whole or individual tabs
 *   - cr-tabs does not accept any <slot> (not necessary as of this writing)
 *   - no horizontal scrolling, it is assumed that tabs always fit in the
 *     available space
 */
import '../hidden_style_css.m.js';
import '../shared_vars_css.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class CrTabsElement extends PolymerElement {
  static get is() {
    return 'cr-tabs';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Tab names displayed in each tab.
       * @type {!Array<string>}
       */
      tabNames: {
        type: Array,
        value: () => [],
      },

      /** Index of the selected tab. */
      selected: {
        type: Number,
        notify: true,
        observer: 'onSelectedChanged_',
      },
    };
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.isRtl_ = false;

    /** @private {?number} */
    this.lastSelected_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.isRtl_ = this.matches(':host-context([dir=rtl]) cr-tabs');
  }

  /** @override */
  ready() {
    super.ready();

    this.setAttribute('role', 'tablist');
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener('mousedown', this.onMouseDown_);
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getAriaSelected_(index) {
    return index === this.selected ? 'true' : 'false';
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getTabindex_(index) {
    return index === this.selected ? '0' : '-1';
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getSelectedClass_(index) {
    return index === this.selected ? 'selected' : '';
  }

  /**
   * @param {number} newSelected
   * @param {number} oldSelected
   * @private
   */
  onSelectedChanged_(newSelected, oldSelected) {
    const tabs = this.shadowRoot.querySelectorAll('.tab');
    if (tabs.length === 0 || oldSelected === undefined) {
      // Tabs are not rendered yet.
      return;
    }

    const oldTabRect = tabs[oldSelected].getBoundingClientRect();
    const newTabRect = tabs[newSelected].getBoundingClientRect();

    const newIndicator = /** @type {!HTMLElement} */ (
        tabs[newSelected].querySelector('.tab-indicator'));
    newIndicator.classList.remove('expand', 'contract');

    // Make new indicator look like it is the old indicator.
    this.updateIndicator_(
        newIndicator, newTabRect, oldTabRect.left, oldTabRect.width);
    newIndicator.getBoundingClientRect();  // Force repaint.

    // Expand to cover both the previous selected tab, the newly selected tab,
    // and everything in between.
    newIndicator.classList.add('expand');
    newIndicator.addEventListener(
        'transitionend', e => this.onIndicatorTransitionEnd_(e), {once: true});
    const leftmostEdge = Math.min(oldTabRect.left, newTabRect.left);
    const fullWidth = newTabRect.left > oldTabRect.left ?
        newTabRect.right - oldTabRect.left :
        oldTabRect.right - newTabRect.left;
    this.updateIndicator_(newIndicator, newTabRect, leftmostEdge, fullWidth);
  }

  /** @private */
  onMouseDown_() {
    this.classList.remove('keyboard-focus');
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    this.classList.add('keyboard-focus');
    const count = this.tabNames.length;
    let newSelection;
    if (e.key === 'Home') {
      newSelection = 0;
    } else if (e.key === 'End') {
      newSelection = count - 1;
    } else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      const delta = e.key === 'ArrowLeft' ? (this.isRtl_ ? 1 : -1) :
                                            (this.isRtl_ ? -1 : 1);
      newSelection = (count + this.selected + delta) % count;
    } else {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selected = newSelection;
    this.shadowRoot.querySelector('.tab.selected').focus();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onIndicatorTransitionEnd_(event) {
    const indicator = event.target;
    indicator.classList.replace('expand', 'contract');
    indicator.style.transform = `translateX(0) scaleX(1)`;
  }

  /**
   * @param {!{model: !{index: number}}} _
   * @private
   */
  onTabClick_({model: {index}}) {
    this.selected = index;
  }

  /**
   * @param {!HTMLElement} indicator
   * @param {!ClientRect} originRect
   * @param {number} newLeft
   * @param {number} newWidth
   * @private
   */
  updateIndicator_(indicator, originRect, newLeft, newWidth) {
    const leftDiff = 100 * (newLeft - originRect.left) / originRect.width;
    const widthRatio = newWidth / originRect.width;
    const transform = `translateX(${leftDiff}%) scaleX(${widthRatio})`;
    indicator.style.transform = transform;
  }
}

customElements.define(CrTabsElement.is, CrTabsElement);
