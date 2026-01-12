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
import {WindowProxy} from './window_proxy.js';

const ThreadsRailElementBase = I18nMixinLit(CrLitElement);

// URL to navigate to the AI Mode history/0-state.
// Parameters:
// - udm=50: Specifies the AI Mode in Google Search.
// - aep=11:  Indicates the AI Mode Entry Point (e.g., from Threads Rail).
// - atvm=1:  Specifies the AI Threads View Mode (e.g., history/0-state view).
export const AI_MODE_HISTORY_URL =
    'https://www.google.com/search?udm=50&aep=11&atvm=1';

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

  protected onShowHistoryClick_(): void {
    // Navigate to the AI Mode search page with history panel.
    WindowProxy.getInstance().navigate(AI_MODE_HISTORY_URL);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-threads-rail': ThreadsRailElement;
  }
}

customElements.define(ThreadsRailElement.is, ThreadsRailElement);
