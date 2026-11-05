// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './threads_rail.css.js';
import {getHtml} from './threads_rail.html.js';
import {WindowProxy} from './window_proxy.js';

/**
 * Enum matching `omnibox::LhsRailConfig::ClickAction::ClientAction` in
 * `third_party/omnibox_proto/lhs_rail_config.proto`.
 */
export enum LhsClientAction {
  UNSPECIFIED = 0,
  NAVIGATE = 1,
  EXPAND_RAIL = 2,
  FOCUS_REALBOX_AIM = 3,
}

export interface LhsRailItem {
  iconSvg: string;
  name: string;
  clientAction: LhsClientAction;
  targetUrl?: string;
}

export interface LhsRailConfig {
  items?: LhsRailItem[];
}

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

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      config_: {type: Object},
    };
  }

  protected accessor config_: LhsRailConfig =
      loadTimeData.getValue('lhsRailConfig');

  protected getItems_(): LhsRailItem[] {
    return this.config_.items ?? [];
  }

  protected getIconStyle_(item: LhsRailItem): string {
    if (!item.iconSvg) {
      return '';
    }
    return `--cr-icon-image: url("data:image/svg+xml;charset=utf-8,${
        encodeURIComponent(item.iconSvg)}");`;
  }

  protected onItemClick_(e: Event): void {
    e.stopPropagation();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const item = this.getItems_()[index];
    if (!item) {
      return;
    }

    switch (item.clientAction) {
      case LhsClientAction.EXPAND_RAIL:
        this.fire('expand-rail');
        return;
      case LhsClientAction.FOCUS_REALBOX_AIM:
        this.fire('open-composebox');
        return;
      case LhsClientAction.NAVIGATE:
      case LhsClientAction.UNSPECIFIED:
      default:
        if (item.targetUrl) {
          WindowProxy.getInstance().navigate(item.targetUrl);
        }
        return;
    }
  }

  protected onLogoClick_(e: Event): void {
    e.stopPropagation();
    const googleBaseUrl = loadTimeData.getString('googleBaseUrl');
    assert(googleBaseUrl);
    WindowProxy.getInstance().navigate(googleBaseUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-threads-rail': ThreadsRailElement;
  }
}

customElements.define(ThreadsRailElement.is, ThreadsRailElement);
