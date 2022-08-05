// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around a button that toggles
 * between an opened (expanded) and closed state.
 */
import '../cr_actionable_row_style.m.js';
import '../cr_icon_button/cr_icon_button.m.js';
import '../icons.m.js';
import '../shared_vars_css.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {focusWithoutInk} from '../../js/cr/ui/focus_without_ink.m.js';

/** @polymer */
export class CrExpandButtonElement extends PolymerElement {
  static get is() {
    return 'cr-expand-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If true, the button is in the expanded state and will show the icon
       * specified in the `collapseIcon` property. If false, the button shows
       * the icon specified in the `expandIcon` property.
       */
      expanded: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onExpandedChange_',
      },

      /**
       * If true, the button will be disabled and grayed out.
       */
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** A11y text descriptor for this control. */
      ariaLabel: {
        type: String,
        observer: 'onAriaLabelChange_',
      },

      tabIndex: {
        type: Number,
        value: 0,
      },

      expandIcon: {
        type: String,
        value: 'cr:expand-more',
        observer: 'onIconChange_',
      },

      collapseIcon: {
        type: String,
        value: 'cr:expand-less',
        observer: 'onIconChange_',
      },

      expandTitle: String,
      collapseTitle: String,

      tooltipText_: {
        type: String,
        computed: 'computeTooltipText_(expandTitle, collapseTitle, expanded)',
        observer: 'onTooltipTextChange_',
      },
    };
  }

  static get observers() {
    return ['updateAriaExpanded_(disabled, expanded)'];
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('click', this.toggleExpand_);
  }

  /**
   * @return {string}
   * @private
   */
  computeTooltipText_() {
    return this.expanded ? this.collapseTitle : this.expandTitle;
  }

  /** @private */
  onTooltipTextChange_() {
    this.title = this.tooltipText_;
  }

  /** @type {boolean} */
  get noink() {
    return this.$.icon.noink;
  }

  /** @type {boolean} */
  set noink(value) {
    this.$.icon.noink = value;
  }

  focus() {
    this.$.icon.focus();
  }

  /** @private */
  onAriaLabelChange_() {
    if (this.ariaLabel) {
      this.$.icon.removeAttribute('aria-labelledby');
      this.$.icon.setAttribute('aria-label', this.ariaLabel);
    } else {
      this.$.icon.removeAttribute('aria-label');
      this.$.icon.setAttribute('aria-labelledby', 'label');
    }
  }

  /** @private */
  onExpandedChange_() {
    this.updateIcon_();
  }

  /** @private */
  onIconChange_() {
    this.updateIcon_();
  }

  /** @private */
  updateIcon_() {
    this.$.icon.ironIcon = this.expanded ? this.collapseIcon : this.expandIcon;
  }

  /**
   * @param {!Event} event
   * @private
   */
  toggleExpand_(event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    event.stopPropagation();
    event.preventDefault();

    this.scrollIntoViewIfNeeded();
    this.expanded = !this.expanded;
    focusWithoutInk(this.$.icon);
  }

  /** @private */
  updateAriaExpanded_() {
    if (this.disabled) {
      this.$.icon.removeAttribute('aria-expanded');
    } else {
      this.$.icon.setAttribute('aria-expanded', this.expanded);
    }
  }
}

customElements.define(CrExpandButtonElement.is, CrExpandButtonElement);
