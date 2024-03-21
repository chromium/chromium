// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_tooltip_icon.css.js';
import {getHtml} from './cr_tooltip_icon.html.js';

export interface CrTooltipIconElement {
  $: {
    indicator: HTMLElement,
  };
}

export class CrTooltipIconElement extends CrLitElement {
  static get is() {
    return 'cr-tooltip-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      iconAriaLabel: {type: String},
      iconClass: {type: String},
      tooltipText: {type: String},

      /** Position of tooltip popup related to the icon. */
      tooltipPosition: {type: String},
    };
  }

  iconAriaLabel: string = '';
  iconClass: string = '';
  tooltipText: string = '';
  tooltipPosition: string = 'top';

  getFocusableElement(): HTMLElement {
    return this.$.indicator;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tooltip-icon': CrTooltipIconElement;
  }
}

customElements.define(CrTooltipIconElement.is, CrTooltipIconElement);
