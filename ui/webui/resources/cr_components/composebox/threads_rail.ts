// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {recordBoolean, recordEnumerationValue} from './common.js';
import {getCss} from './threads_rail.css.js';
import {getHtml} from './threads_rail.html.js';
import {WindowProxy} from './window_proxy.js';

// LINT.IfChange(NTPThreadsRailAction)
/**
 * User actions on the threads rail. This enum must match the numbering for
 * NTPThreadsRailAction in enums.xml. These values are persisted to logs.
 * Entries should not be renumbered, removed or reused.
 */
export enum ThreadsRailAction {
  // 0 was legacy "Show history clicked" (obsolete).
  EXPAND_RAIL_ITEM_CLICKED = 1,
  FOCUS_REALBOX_AIM_ITEM_CLICKED = 2,
  NAVIGATE_ITEM_CLICKED = 3,
  LOGO_CLICKED = 4,
  RAIL_BACKGROUND_CLICKED = 5,
  UNSPECIFIED_NAVIGATE_ITEM_CLICKED = 6,
  FALLBACK_NAVIGATE_ITEM_CLICKED = 7,
  MAX_VALUE = FALLBACK_NAVIGATE_ITEM_CLICKED,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NTPThreadsRailAction)

function recordThreadsRailAction(action: ThreadsRailAction) {
  recordEnumerationValue(
      'NewTabPage.ThreadsRail.Action', action, ThreadsRailAction.MAX_VALUE + 1);
}

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
      inert: {
        type: Boolean,
        reflect: true,
      },
      suppressPointerHover_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  override accessor inert: boolean = false;
  private accessor config_: LhsRailConfig =
      loadTimeData.getValue('lhsRailConfig');
  protected accessor suppressPointerHover_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    recordBoolean('NewTabPage.ThreadsRail.Shown', true);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.unsuppressPointerHover_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (!changedProperties.has('inert')) {
      return;
    }

    this.unsuppressPointerHover_();
    // Suppress pointer hit-testing when leaving inert until the next real
    // pointer event so Blink's post-layout hover recomputation at a stale
    // pre-OOPIF mouse position does not re-trigger :hover or tooltips on
    // collapse.
    this.suppressPointerHover_ =
        !this.inert && changedProperties.get('inert') === true;
    if (this.suppressPointerHover_) {
      this.eventTracker_.add(
          window, 'pointermove', () => this.unsuppressPointerHover_());
      this.eventTracker_.add(
          window, 'pointerdown', () => this.unsuppressPointerHover_());
    }
  }

  private unsuppressPointerHover_(): void {
    this.eventTracker_.removeAll();
    this.suppressPointerHover_ = false;
  }

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

  protected onRailClick_(e: Event): void {
    if (e.target !== e.currentTarget) {
      e.stopPropagation();
      return;
    }
    recordThreadsRailAction(ThreadsRailAction.RAIL_BACKGROUND_CLICKED);
  }

  protected onItemClick_(e: Event): void {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const item = this.getItems_()[index];
    if (!item) {
      return;
    }

    switch (item.clientAction) {
      case LhsClientAction.EXPAND_RAIL:
        recordThreadsRailAction(ThreadsRailAction.EXPAND_RAIL_ITEM_CLICKED);
        this.fire('expand-rail');
        return;
      case LhsClientAction.FOCUS_REALBOX_AIM:
        recordThreadsRailAction(
            ThreadsRailAction.FOCUS_REALBOX_AIM_ITEM_CLICKED);
        this.fire('open-composebox');
        return;
      case LhsClientAction.NAVIGATE:
        if (item.targetUrl) {
          recordThreadsRailAction(ThreadsRailAction.NAVIGATE_ITEM_CLICKED);
          WindowProxy.getInstance().navigate(item.targetUrl);
        }
        return;
      case LhsClientAction.UNSPECIFIED:
        if (item.targetUrl) {
          recordThreadsRailAction(
              ThreadsRailAction.UNSPECIFIED_NAVIGATE_ITEM_CLICKED);
          WindowProxy.getInstance().navigate(item.targetUrl);
        }
        return;
      default:
        if (item.targetUrl) {
          recordThreadsRailAction(
              ThreadsRailAction.FALLBACK_NAVIGATE_ITEM_CLICKED);
          WindowProxy.getInstance().navigate(item.targetUrl);
        }
        return;
    }
  }

  protected onLogoClick_(): void {
    const googleBaseUrl = loadTimeData.getString('googleBaseUrl');
    assert(googleBaseUrl);
    recordThreadsRailAction(ThreadsRailAction.LOGO_CLICKED);
    WindowProxy.getInstance().navigate(googleBaseUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-threads-rail': ThreadsRailElement;
  }
}

customElements.define(ThreadsRailElement.is, ThreadsRailElement);
