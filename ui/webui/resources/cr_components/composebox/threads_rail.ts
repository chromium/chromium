// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {recordEnumerationValue} from './common.js';
import {getCss} from './threads_rail.css.js';
import {getHtml} from './threads_rail.html.js';
import {WindowProxy} from './window_proxy.js';

/**
 * User actions on the threads rail. This enum must match the numbering for
 * NtpThreadsAction in enums.xml. These values are persisted to logs.
 * Entries should not be renumbered, removed or reused.
 */
export enum ThreadsAction {
  SHOW_HISTORY_CLICKED = 0,
  MAX_VALUE = SHOW_HISTORY_CLICKED,
}

function recordAction(action: ThreadsAction) {
  recordEnumerationValue(
      'NewTabPage.ThreadsRail.Action', action, ThreadsAction.MAX_VALUE + 1);
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
      displayLogo_: {type: Boolean},
    };
  }

  protected accessor displayLogo_: boolean =
      loadTimeData.getBoolean('enableThreadsRailLogo');

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
    recordAction(ThreadsAction.SHOW_HISTORY_CLICKED);
    // Navigate to the AI Mode search page. This will be intercepted
    // by the co-browse service if the contextual task flag is enabled.
    const threadsUrl = loadTimeData.getString('threadsUrl');
    assert(threadsUrl);
    WindowProxy.getInstance().navigate(threadsUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-threads-rail': ThreadsRailElement;
  }
}

customElements.define(ThreadsRailElement.is, ThreadsRailElement);
