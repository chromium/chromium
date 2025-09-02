// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './context_menu_entrypoint.css.js';
import {getHtml} from './context_menu_entrypoint.html.js';

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
      hasTabSuggestions_: {type: Boolean},
    };
  }

  // TODO(crbug.com/442575942): Set `hasTabSuggestions_` to false by default,
  // and add actual logic for setting it true.
  protected accessor hasTabSuggestions_: boolean = true;

  protected onEntrypointClick_() {
    this.$.menu.showAt(this.$.entrypointIcon, {
      top: this.$.entrypointIcon.getBoundingClientRect().bottom,
      width: 190,
      anchorAlignmentX: AnchorAlignment['AFTER_START'],
    });
  }

  protected openImageUpload_() {
    // TODO(crbug.com/439618274):  Integrate existing file and photo context
    // behavior into new context menu.
    this.$.menu.close();
  }

  protected openFileUpload_() {
    // TODO(crbug.com/439618274):  Integrate existing file and photo context
    // behavior into new context menu.
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
