// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_tool_chip.css.js';
import {getHtml} from './composebox_tool_chip.html.js';

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-tool-chip': ComposeboxToolChipElement;
  }
}

export class ComposeboxToolChipElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-tool-chip';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      icon: {type: String},
      label: {type: String},
      visible: {type: Boolean},
      removeChipAriaLabel: {type: String},
    };
  }
  protected accessor icon:string = '';
  protected accessor label:string = '';
  protected accessor visible:boolean = false;
  protected accessor removeChipAriaLabel: string = '';

  override render() {
    if (!this.visible) {
      return;
    }
    return getHtml.call(this);
  }
}

customElements.define(ComposeboxToolChipElement.is, ComposeboxToolChipElement);
