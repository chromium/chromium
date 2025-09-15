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
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageHandlerRemote, TabInfo} from './composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {getCss} from './context_menu_entrypoint.css.js';
import {getHtml} from './context_menu_entrypoint.html.js';

/** The width of the dropdown menu in pixels. */
const MENU_WIDTH_PX = 190;

export interface ContextMenuEntrypointElement {
  $: {
    entrypoint: HTMLElement,
    entrypointIcon: HTMLElement,
    menu: CrActionMenuElement,
  };
}

export class ContextMenuEntrypointElement extends CrLitElement {
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
      tabSuggestions_: {type: Array},
    };
  }

  accessor inputsDisabled: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  private pageHandler_: PageHandlerRemote;

  constructor() {
    super();

    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
  }

  protected async onEntrypointClick_() {
    const {tabs} = await this.pageHandler_.getTabs();
    this.tabSuggestions_ = tabs;

    this.$.menu.showAt(this.$.entrypointIcon, {
      top: this.$.entrypointIcon.getBoundingClientRect().bottom,
      width: MENU_WIDTH_PX,
      anchorAlignmentX: AnchorAlignment['AFTER_START'],
    });
  }

  protected addTabContext(e: Event) {
    e.stopPropagation();

    const tabElement = e.target! as HTMLInputElement;
    const tabId = Number(tabElement.dataset['id']);
    if (!tabId) {
      return;
    }

    this.fire('add-tab-context', {
      id: tabId,
      title: tabElement.dataset['title']!,
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
