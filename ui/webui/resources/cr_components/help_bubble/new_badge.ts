// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A badge that can be added to UI elements to indicate newness.
 * This SHOULD NOT BE USED by new features until the below follow up is done!
 *
 * Unlike the Views version, this does not automatically disappear after the
 * feature is no longer new, so this must be done manually.
 * TODO(crbug.com/361169212): Follow up to integrate with auto-disappear code.
 */

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './new_badge.css.js';
import {getHtml} from './new_badge.html.js';

export interface NewBadgeElement {
  $: {};
}

const NewBadgeElementBase = I18nMixinLit(CrLitElement);

export class NewBadgeElement extends NewBadgeElementBase {
  static get is() {
    return 'new-badge';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-badge': NewBadgeElement;
  }
}

customElements.define(NewBadgeElement.is, NewBadgeElement);
