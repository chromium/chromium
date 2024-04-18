// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 */
import '../cr_icon_button/cr_icon_button.js';
import '../cr_icon/cr_icon.js';
import '../icons_lit.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getCss} from './cr_link_row.css.js';
import {getHtml} from './cr_link_row.html.js';

export interface CrLinkRowElement {
  $: {
    icon: CrIconButtonElement,
    buttonAriaDescription: HTMLElement,
  };
}

export class CrLinkRowElement extends CrLitElement {
  static get is() {
    return 'cr-link-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaShowLabel: {
        type: Boolean,
        reflect: true,
      },

      ariaShowSublabel: {
        type: Boolean,
        reflect: true,
      },

      startIcon: {type: String},
      label: {type: String},
      subLabel: {type: String},

      disabled: {
        type: Boolean,
        reflect: true,
      },

      external: {type: Boolean},
      usingSlottedLabel: {type: Boolean},
      roleDescription: {type: String},
      buttonAriaDescription: {type: String},
    };
  }

  ariaShowLabel: boolean = false;
  ariaShowSublabel: boolean = false;
  startIcon: string = '';
  label: string = '';
  subLabel: string = '';
  disabled: boolean = false;
  external: boolean = false;
  usingSlottedLabel: boolean = false;
  roleDescription?: string;
  buttonAriaDescription?: string;

  override focus() {
    this.$.icon.focus();
  }

  protected shouldHideLabelWrapper_(): boolean {
    return !(this.label || this.usingSlottedLabel);
  }

  protected getIcon_(): string {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  }

  protected getButtonAriaDescription_(): string {
    return this.buttonAriaDescription ??
        (this.external ? loadTimeData.getString('opensInNewTab') : '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-link-row': CrLinkRowElement;
  }
}

customElements.define(CrLinkRowElement.is, CrLinkRowElement);
