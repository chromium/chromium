// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MouseHoverableMixin} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

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
