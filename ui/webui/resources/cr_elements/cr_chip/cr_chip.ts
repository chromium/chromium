// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PaperRippleBehavior} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_chip.html.js';

const CrChipElementBase =
    mixinBehaviors([PaperRippleBehavior], PolymerElement) as {
      new (): PolymerElement & PaperRippleBehavior,
    };

export class CrChip extends CrChipElementBase {
  static get is() {
    return 'cr-chip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,
      chipRole: String,
      selected: Boolean,
    };
  }

  disabled: boolean;
  chipRole: string;
  selected: boolean;

  constructor() {
    super();
    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
    }
  }

  private onPointerDown_() {
    this.ensureRipple();
  }

  // Overridden from PaperRippleBehavior
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.shadowRoot!.querySelector('button');
    return super._createRipple();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-chip': CrChip;
  }
}

customElements.define(CrChip.is, CrChip);
