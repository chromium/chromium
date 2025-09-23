// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './composebox_tab_favicon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './context_menu_entrypoint.css.js';
import {getHtml} from './context_menu_entrypoint.html.js';

/** The width of the dropdown menu in pixels. */
const MENU_WIDTH_PX = 190;

export interface ContextMenuEntrypointElement {
  $: {
    entrypoint: HTMLElement,
    menu: CrActionMenuElement,
  };
}

const ContextMenuEntrypointElementBase = I18nMixinLit(CrLitElement);

export class ContextMenuEntrypointElement extends
    ContextMenuEntrypointElementBase {
  static get is() {
    return 'composebox-context-menu-entrypoint';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      inputsDisabled: {type: Boolean},
      showEntrypointDescription: {type: Boolean},
      tabSuggestions: {type: Array},
    };
  }

  accessor inputsDisabled: boolean = false;
  accessor showEntrypointDescription: boolean;
  accessor tabSuggestions: TabInfo[] = [];

  constructor() {
    super();
  }

  protected onEntrypointClick_() {
    this.fire('refresh-tab-suggestions', {onRefreshComplete: () => {
      this.$.menu.showAt(this.$.entrypoint, {
        top: this.$.entrypoint.getBoundingClientRect().bottom,
        width: MENU_WIDTH_PX,
        anchorAlignmentX: AnchorAlignment['AFTER_START'],
      });
    }});
  }

  protected addTabContext(e: Event) {
    e.stopPropagation();

    const tabElement = e.currentTarget! as HTMLButtonElement;
    const tabInfo = this.tabSuggestions[Number(tabElement.dataset['index'])];

    if (!tabInfo) {
      return;
    }

    this.fire('add-tab-context', {
      id: tabInfo.tabId,
      title: tabInfo.title,
      url: tabInfo.url,
    });
    this.$.menu.close();
  }

  protected openImageUpload() {
    this.fire('open-image-upload');
    this.$.menu.close();
  }

  protected openFileUpload() {
    this.fire('open-file-upload');
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-context-menu-entrypoint': ContextMenuEntrypointElement;
  }
}

customElements.define(
    ContextMenuEntrypointElement.is, ContextMenuEntrypointElement);
