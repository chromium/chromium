// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cluster_menu.css.js';
import {getHtml} from './cluster_menu.html.js';

/**
 * @fileoverview This file provides a custom element displaying an action menu.
 * It's meant to be flexible enough to be associated with either a specific
 * visit, or the whole cluster, or the top visit of unlabelled cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'cluster-menu': ClusterMenuElement;
  }
}

const ClusterMenuElementBase = I18nMixinLit(CrLitElement);

export interface ClusterMenuElement {
  $: {
    actionMenuButton: HTMLElement,
  };
}

export class ClusterMenuElement extends ClusterMenuElementBase {
  static get is() {
    return 'cluster-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Usually this is true, but this can be false if deleting history is
       * prohibited by Enterprise policy.
       */
      allowDeletingHistory_: {type: Boolean},

      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        reflect: true,
      },

      renderActionMenu_: {type: Boolean},
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  protected allowDeletingHistory_: boolean =
      loadTimeData.getBoolean('allowDeletingHistory');
  protected inSidePanel_: boolean = loadTimeData.getBoolean('inSidePanel');
  protected renderActionMenu_: boolean = false;

  //============================================================================
  // Event handlers
  //============================================================================

  protected async onActionMenuButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).
    if (!this.renderActionMenu_) {
      this.renderActionMenu_ = true;
      await this.updateComplete;
    }
    const menu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(menu);
    menu.showAt(this.$.actionMenuButton);
  }

  protected onOpenAllButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.fire('open-all-visits');

    this.closeActionMenu_();
  }

  protected onHideAllButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.fire('hide-all-visits');

    this.closeActionMenu_();
  }

  protected onRemoveAllButtonClick_(event: Event) {
    event.preventDefault();  // Prevent default browser action (navigation).

    this.fire('remove-all-visits');

    this.closeActionMenu_();
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private closeActionMenu_() {
    const menu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(menu);
    menu.close();
  }
}

customElements.define(ClusterMenuElement.is, ClusterMenuElement);
