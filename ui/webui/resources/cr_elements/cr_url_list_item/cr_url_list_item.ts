// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icons.css.js';
import '../cr_shared_vars.css.js';

import {assert} from '//resources/js/assert_ts.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MouseHoverableMixin} from '../mouse_hoverable_mixin.js';

import {getTemplate} from './cr_url_list_item.html.js';

export enum CrUrlListItemSize {
  COMPACT = 'compact',
  MEDIUM = 'medium',
  LARGE = 'large',
}

const CrUrlListItemElementBase = MouseHoverableMixin(PolymerElement);

export class CrUrlListItemElement extends CrUrlListItemElementBase {
  static get is() {
    return 'cr-url-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      alwaysShowSuffixIcons: {
        reflectToAttribute: true,
        type: Boolean,
      },
      count: Number,
      size: {
        observer: 'onSizeChanged_',
        reflectToAttribute: true,
        type: String,
        value: CrUrlListItemSize.MEDIUM,
      },
      url: String,
    };
  }

  count?: number;
  iconsAlwaysVisible: boolean;
  size: CrUrlListItemSize;
  url?: string;

  override ready() {
    super.ready();
    FocusOutlineManager.forDocument(document);
  }

  private getFavicon_(): string {
    return getFaviconForPageURL(this.url || '', false);
  }

  private onSizeChanged_() {
    assert(Object.values(CrUrlListItemSize).includes(this.size));
  }

  private shouldShowFavicon_(): boolean {
    return this.url !== undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-url-list-item': CrUrlListItemElement;
  }
}

customElements.define(CrUrlListItemElement.is, CrUrlListItemElement);
