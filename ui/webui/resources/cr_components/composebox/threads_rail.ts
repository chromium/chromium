// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './threads_rail.css.js';
import {getHtml} from './threads_rail.html.js';

const ThreadsRailElementBase = I18nMixinLit(CrLitElement);

/**
 * The element for displaying the AI Mode threads rail.
 */
export class ThreadsRailElement extends ThreadsRailElementBase {
  static get is() {
    return 'cr-threads-rail';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {};
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  // TODO(crbug.com/474444031): Add logo, and buttons & tooltips to threads
  // rail.
  protected onShowHistoryClick_(): void {}
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-threads-rail': ThreadsRailElement;
  }
}

customElements.define(ThreadsRailElement.is, ThreadsRailElement);
