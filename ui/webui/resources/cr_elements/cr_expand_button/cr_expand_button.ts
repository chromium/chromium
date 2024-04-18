// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around a button that toggles
 * between an opened (expanded) and closed state.
 */
import '../cr_icon_button/cr_icon_button.js';
import '../icons_lit.html.js';

import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getCss} from './cr_expand_button.css.js';
import {getHtml} from './cr_expand_button.html.js';

export interface CrExpandButtonElement {
  $: {
    icon: CrIconButtonElement,
  };
}

export class CrExpandButtonElement extends CrLitElement {
  static get is() {
    return 'cr-expand-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * If true, the button is in the expanded state and will show the icon
       * specified in the `collapseIcon` property. If false, the button shows
       * the icon specified in the `expandIcon` property.
       */
      expanded: {
        type: Boolean,
        notify: true,
      },

      /**
       * If true, the button will be disabled and grayed out.
       */
      disabled: {
        type: Boolean,
        reflect: true,
      },

      /** A11y text descriptor for this control. */
      ariaLabel: {type: String},

      tabIndex: {type: Number},
      expandIcon: {type: String},
      collapseIcon: {type: String},
      expandTitle: {type: String},
      collapseTitle: {type: String},
    };
  }

  expanded: boolean = false;
  disabled: boolean = false;
  expandIcon: string = 'cr:expand-more';
  collapseIcon: string = 'cr:expand-less';
  expandTitle?: string;
  collapseTitle?: string;
  override tabIndex: number = 0;

  override firstUpdated() {
    this.addEventListener('click', this.toggleExpand_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('expanded') ||
        changedProperties.has('collapseTitle') ||
        changedProperties.has('expandTitle')) {
      this.title =
          (this.expanded ? this.collapseTitle : this.expandTitle) || '';
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('ariaLabel')) {
      this.onAriaLabelChange_();
    }
  }

  override focus() {
    this.$.icon.focus();
  }

  protected getIcon_(): string {
    return this.expanded ? this.collapseIcon : this.expandIcon;
  }

  protected getAriaExpanded_(): string {
    return this.expanded ? 'true' : 'false';
  }

  private onAriaLabelChange_() {
    if (this.ariaLabel) {
      this.$.icon.removeAttribute('aria-labelledby');
      this.$.icon.setAttribute('aria-label', this.ariaLabel);
    } else {
      this.$.icon.removeAttribute('aria-label');
      this.$.icon.setAttribute('aria-labelledby', 'label');
    }
  }

  private toggleExpand_(event: Event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    event.stopPropagation();
    event.preventDefault();

    this.scrollIntoViewIfNeeded();
    this.expanded = !this.expanded;
    focusWithoutInk(this.$.icon);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-expand-button': CrExpandButtonElement;
  }
}

customElements.define(CrExpandButtonElement.is, CrExpandButtonElement);
