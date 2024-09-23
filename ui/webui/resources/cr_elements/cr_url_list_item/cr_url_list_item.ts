// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {assert} from '//resources/js/assert.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {MouseHoverableMixinLit} from '../mouse_hoverable_mixin_lit.js';

import {getCss} from './cr_url_list_item.css.js';
import {getHtml} from './cr_url_list_item.html.js';

export enum CrUrlListItemSize {
  COMPACT = 'compact',
  MEDIUM = 'medium',
  LARGE = 'large',
}

export interface CrUrlListItemElement {
  $: {
    anchor: HTMLAnchorElement,
    badgesContainer: HTMLElement,
    badges: HTMLSlotElement,
    button: HTMLElement,
    content: HTMLSlotElement,
    description: HTMLSlotElement,
    metadata: HTMLElement,
  };
}

const CrUrlListItemElementBase = MouseHoverableMixinLit(CrLitElement);

export class CrUrlListItemElement extends CrUrlListItemElementBase {
  static get is() {
    return 'cr-url-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      alwaysShowSuffix: {
        type: Boolean,
        reflect: true,
      },
      itemAriaLabel: {type: String},
      itemAriaDescription: {type: String},
      count: {type: Number},
      description: {type: String},
      url: {type: String},

      title: {
        reflect: true,
        type: String,
      },

      hasBadges: {
        type: Boolean,
        reflect: true,
      },

      hasDescriptions_: {
        type: Boolean,
        reflect: true,
      },

      hasSlottedContent_: {
        type: Boolean,
        reflect: true,
      },

      reverseElideDescription: {
        type: Boolean,
        reflect: true,
      },

      isFolder_: {
        type: Boolean,
        reflect: true,
      },

      size: {
        type: String,
        reflect: true,
      },

      imageUrls: {type: Array},

      firstImageLoaded_: {
        type: Boolean,
        state: true,
      },

      forceHover: {
        reflect: true,
        type: Boolean,
      },

      descriptionMeta: {type: String},

      /**
       * Flag that determines if the element should use an anchor tag or a
       * button element as its focusable item. An anchor provides the native
       * context menu and browser interactions for links, while a button
       * provides its own unique functionality, such as pressing space to
       * activate.
       */
      asAnchor: {type: Boolean},
      asAnchorTarget: {type: String},
    };
  }

  alwaysShowSuffix: boolean = false;
  asAnchor: boolean = false;
  asAnchorTarget: string = '_self';
  itemAriaLabel?: string;
  itemAriaDescription?: string;
  count?: number;
  description?: string;
  reverseElideDescription: boolean = false;
  hasBadges: boolean = false;
  protected hasDescriptions_: boolean = false;
  protected hasSlottedContent_: boolean = false;
  protected isFolder_: boolean = false;
  size: CrUrlListItemSize = CrUrlListItemSize.MEDIUM;
  override title: string = '';
  url?: string;
  imageUrls: string[] = [];
  protected firstImageLoaded_: boolean = false;
  forceHover: boolean = false;
  descriptionMeta: string = '';

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    FocusOutlineManager.forDocument(document);
    this.addEventListener('pointerdown', () => this.setActiveState_(true));
    this.addEventListener('pointerup', () => this.setActiveState_(false));
    this.addEventListener('pointerleave', () => this.setActiveState_(false));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('hasBadges') ||
        changedProperties.has('description')) {
      this.hasDescriptions_ =
          !!this.description || this.hasBadges || !!this.descriptionMeta;
    }

    if (changedProperties.has('count')) {
      this.isFolder_ = this.count !== undefined;
    }

    if (changedProperties.has('size')) {
      assert(Object.values(CrUrlListItemSize).includes(this.size));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('imageUrls')) {
      this.resetFirstImageLoaded_();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.resetFirstImageLoaded_();
  }

  override focus() {
    this.getFocusableElement().focus();
  }

  getFocusableElement() {
    if (this.asAnchor) {
      return this.$.anchor;
    } else {
      return this.$.button;
    }
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

  protected getItemAriaDescription_(): string|undefined {
    return this.itemAriaDescription || this.description;
  }

  protected getItemAriaLabel_(): string {
    return this.itemAriaLabel || this.title;
  }

  protected getDisplayedCount_(): string {
    if (this.count && this.count > 999) {
      // The square to display the count only fits 3 characters.
      return '99+';
    }

    return this.count === undefined ? '' : this.count.toString();
  }

  protected getFavicon_(): string {
    return getFaviconForPageURL(this.url || '', false);
  }

  protected shouldShowImageUrl_(_url: string, index: number): boolean {
    return index <= 1;
  }

  protected onBadgesSlotChange_() {
    this.hasBadges = this.$.badges.assignedElements({flatten: true}).length > 0;
  }

  protected onContentSlotChange_() {
    this.hasSlottedContent_ =
        this.$.content.assignedElements({flatten: true}).length > 0;
  }

  private setActiveState_(active: boolean) {
    this.classList.toggle('active', active);
  }

  protected shouldShowFavicon_(): boolean {
    return this.url !== undefined &&
        (this.size === CrUrlListItemSize.COMPACT ||
         this.imageUrls.length === 0);
  }

  protected shouldShowUrlImage_(): boolean {
    return this.url !== undefined &&
        !(this.size === CrUrlListItemSize.COMPACT ||
          this.imageUrls.length === 0) &&
        this.firstImageLoaded_;
  }

  protected shouldShowFolderImages_(): boolean {
    return this.size !== CrUrlListItemSize.COMPACT;
  }

  protected shouldShowFolderIcon_(): boolean {
    return this.size === CrUrlListItemSize.COMPACT ||
        this.imageUrls.length === 0;
  }

  protected shouldShowFolderCount_(): boolean {
    return this.url === undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-url-list-item': CrUrlListItemElement;
  }
}

customElements.define(CrUrlListItemElement.is, CrUrlListItemElement);
