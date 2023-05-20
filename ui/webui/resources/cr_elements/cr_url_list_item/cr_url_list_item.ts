// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_hidden_style.css.js';
import '../cr_icons.css.js';
import '../cr_shared_vars.css.js';
import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

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

export interface CrUrlListItemElement {
  $: {
    badges: HTMLSlotElement,
    content: HTMLSlotElement,
    description: HTMLSlotElement,
    title: HTMLButtonElement,
  };
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
      buttonAriaLabel: String,
      buttonAriaDescription: String,
      count: Number,
      description: String,
      url: String,

      title: {
        reflectToAttribute: true,
        type: String,
      },

      hasBadges_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      hasDescriptions_: {
        type: Boolean,
        computed: 'computeHasDescriptions_(hasBadges_, description)',
        reflectToAttribute: true,
      },

      hasSlottedContent_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      reverseElideDescription: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      isFolder_: {
        computed: 'computeIsFolder_(count)',
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      size: {
        observer: 'onSizeChanged_',
        reflectToAttribute: true,
        type: String,
        value: CrUrlListItemSize.MEDIUM,
      },

      imageUrls: {
        observer: 'resetFirstImageLoaded_',
        type: Array,
        value: () => [],
      },

      firstImageLoaded_: {
        type: Boolean,
        value: false,
      },

      forceHover: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      timestamp: {
        type: String,
        value: '',
      },
    };
  }

  buttonAriaLabel?: string;
  buttonAriaDescription?: string;
  count?: number;
  description?: string;
  reverseElideDescription: boolean;
  private hasBadges_: boolean;
  private hasDescription_: boolean;
  private hasSlottedContent_: boolean;
  private isFolder_: boolean;
  size: CrUrlListItemSize;
  url?: string;
  imageUrls: string[];
  private firstImageLoaded_: boolean;
  forceHover: boolean;
  timestamp: string;

  override ready() {
    super.ready();
    FocusOutlineManager.forDocument(document);
    this.addEventListener('pointerdown', () => this.setActiveState_(true));
    this.addEventListener('pointerup', () => this.setActiveState_(false));
    this.addEventListener('pointerleave', () => this.setActiveState_(false));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.resetFirstImageLoaded_();
  }

  override focus() {
    // This component itself is not focusable, so override its focus method
    // to focus its main focusable child, the title button.
    this.$.title.focus();
  }

  private resetFirstImageLoaded_() {
    this.firstImageLoaded_ = false;
    const image = this.shadowRoot!.querySelector('img');
    if (!image) {
      return;
    }

    if (image.complete) {
      this.firstImageLoaded_ = true;
      return;
    }

    image.addEventListener('load', () => {
      this.firstImageLoaded_ = true;
    }, {once: true});
  }

  private computeHasDescriptions_(): boolean {
    return !!this.description || this.hasBadges_ || !!this.timestamp;
  }

  private computeIsFolder_(): boolean {
    return this.count !== undefined;
  }

  private getButtonAriaDescription_(): string|undefined {
    return this.buttonAriaDescription || this.description;
  }

  private getButtonAriaLabel_(): string {
    return this.buttonAriaLabel || this.title;
  }

  private getDisplayedCount_() {
    if (this.count && this.count > 999) {
      // The square to display the count only fits 3 characters.
      return '99+';
    }

    return this.count;
  }

  private getFavicon_(): string {
    return getFaviconForPageURL(this.url || '', false);
  }

  private shouldShowImageUrl_(_url: string, index: number) {
    return index <= 1;
  }

  private onBadgesSlotChange_() {
    this.hasBadges_ =
        this.$.badges.assignedElements({flatten: true}).length > 0;
  }

  private onContentSlotChange_() {
    this.hasSlottedContent_ =
        this.$.content.assignedElements({flatten: true}).length > 0;
  }

  private onSizeChanged_() {
    assert(Object.values(CrUrlListItemSize).includes(this.size));
  }

  private setActiveState_(active: boolean) {
    this.classList.toggle('active', active);
  }

  private shouldShowFavicon_(): boolean {
    return this.url !== undefined &&
        (this.size === CrUrlListItemSize.COMPACT ||
         this.imageUrls.length === 0);
  }

  private shouldShowUrlImage_(): boolean {
    return this.url !== undefined &&
        !(this.size === CrUrlListItemSize.COMPACT ||
          this.imageUrls.length === 0) &&
        this.firstImageLoaded_;
  }

  private shouldShowFolderImages_(): boolean {
    return this.size !== CrUrlListItemSize.COMPACT;
  }

  private shouldShowFolderIcon_(): boolean {
    return this.size === CrUrlListItemSize.COMPACT ||
        this.imageUrls.length === 0;
  }

  private shouldShowFolderCount_(): boolean {
    return this.url === undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-url-list-item': CrUrlListItemElement;
  }
}

customElements.define(CrUrlListItemElement.is, CrUrlListItemElement);
