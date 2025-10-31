// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconUrl} from '//resources/js/icon.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox_icon.css.js';
import {getHtml} from './searchbox_icon.html.js';

const CALCULATOR: string = 'search-calculator-answer';
const DOCUMENT_MATCH_TYPE: string = 'document';
const FEATURED_ENTERPRISE_SEARCH: string = 'featured-enterprise-search';
const HISTORY_CLUSTER_MATCH_TYPE: string = 'history-cluster';
const PEDAL: string = 'pedal';
const STARTER_PACK: string = 'starter-pack';

export interface SearchboxIconElement {
  $: {
    container: HTMLElement,
    image: HTMLImageElement,
    icon: HTMLElement,
    faviconImageContainer: HTMLElement,
    faviconImage: HTMLImageElement,
    iconImg: HTMLImageElement,
  };
}

// The LHS icon. Used on autocomplete matches as well as the searchbox input to
// render icons, favicons, and entity images.
export class SearchboxIconElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * The default icon to show when no match is selected and/or for
       * non-navigation matches. Only set in the context of the searchbox input.
       */
      defaultIcon: {type: String},

      /**  Whether icon should have a background. */
      hasIconContainerBackground: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether icon is in searchbox or not. Used to prevent
       * the match icon of rich suggestions from showing in the context of the
       * searchbox input.
       */
      inSearchbox: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether icon belongs to an answer or not. Used to prevent
       * the match image from taking size of container.
       */
      isAnswer: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether icon belongs to a starter pack match.
       */
      isStarterPack: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether icon belongs to a featured enterprise search match.
       */
      isFeaturedEnterpriseSearch: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether suggestion answer is of answer type weather. Weather answers
       * don't have the same background as other suggestion answers.
       */
      isWeatherAnswer: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether suggestion is an enterprise search aggregator people
       * suggestion. Enterprise search aggregator people suggestions should not
       * use a set background color when image is missing, unlike other rich
       * suggestion answers.
       */
      isEnterpriseSearchAggregatorPeopleType: {
        type: Boolean,
        reflect: true,
      },

      /** Used as a mask image on #icon if `faviconImage_` is empty. */
      maskImage: {
        type: String,
        reflect: true,
      },

      match: {type: Object},

      //========================================================================
      // Private properties
      //========================================================================

      /** Used as the image src for the #faviconImage if non-empty. */
      faviconImage_: {
        type: String,
        reflect: true,
      },

      /**
       * Used as the image srcset for the #faviconImage if non-empty.
       */
      faviconImageSrcSet_: {state: true, type: String},

      /**
       * Whether the match features an image (as opposed to an icon or favicon).
       */
      hasImage_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether to use the favicon image instead of the default vector icon
       * for the suggestion.
       */
      showFaviconImage_: {state: true, type: Boolean},

      /**
       * Flag indicating whether or not a favicon is loading.
       */
      faviconLoading_: {state: true, type: Boolean},

      /**
       * Flag indicating whether or not a favicon was successfully loaded.
       * This is used to force the WebUI popup to make use of the default vector
       * icon when the favicon image is unavailable.
       */
      faviconError_: {state: true, type: Boolean},

      /** Used as the image src for the #iconImg if non-empty. */
      iconSrc_: {state: true, type: String},

      /**
       * Flag indicating whether or not an icon image is loading. This is used
       * to show a default icon while the image is loading.
       */
      iconLoading_: {state: true, type: Boolean},

      /**
       * Whether to use the icon image instead of the default icon for the
       * suggestion.
       */
      showIconImg_: {state: true, type: Boolean},

      showImage_: {state: true, type: Boolean},
      imageSrc_: {state: true, type: String},

      /**
       * Flag indicating whether or not an image is loading. This is used to
       * show a placeholder color while the image is loading.
       */
      imageLoading_: {state: true, type: Boolean},

      /**
       * Flag indicating whether or not an image was successfully loaded. This
       * is used to suppress the default "broken image" icon as needed.
       */
      imageError_: {
        state: true,
        type: Boolean,
      },

      isTopChromeSearchbox_: {state: true, type: Boolean},

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor defaultIcon: string = '';
  accessor hasIconContainerBackground: boolean = false;
  accessor inSearchbox: boolean = false;
  accessor isAnswer: boolean = false;
  accessor isStarterPack = false;
  accessor isFeaturedEnterpriseSearch = false;
  accessor isWeatherAnswer: boolean = false;
  accessor isEnterpriseSearchAggregatorPeopleType: boolean = false;
  accessor maskImage: string = '';
  accessor match: AutocompleteMatch|null = null;
  protected accessor faviconImage_: string = '';
  protected accessor faviconImageSrcSet_: string = '';
  protected accessor hasImage_: boolean = false;
  protected accessor showFaviconImage_: boolean = false;
  private accessor faviconLoading_: boolean = false;
  private accessor faviconError_: boolean = false;
  protected accessor iconSrc_: string = '';
  private accessor iconLoading_: boolean = false;
  protected accessor showIconImg_: boolean = false;
  protected accessor showImage_: boolean = false;
  protected accessor imageSrc_: string = '';
  private accessor imageLoading_: boolean = false;
  private accessor imageError_: boolean = false;
  private accessor isTopChromeSearchbox_: boolean =
      loadTimeData.getBoolean('isTopChromeSearchbox');
  private accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('match')) {
      this.iconSrc_ = this.computeIconSrc_();
      this.imageSrc_ = this.computeImageSrc_();
      this.isAnswer = this.computeIsAnswer_();
      this.isEnterpriseSearchAggregatorPeopleType =
          this.computeIsEnterpriseSearchAggregatorPeopleType_();
      this.isStarterPack = this.computeIsStarterPack_();
      this.isFeaturedEnterpriseSearch =
          this.computeIsFeaturedEnterpriseSearch();
      this.isWeatherAnswer = this.computeIsWeatherAnswer_();
      this.hasImage_ = this.computeHasImage_();
      this.maskImage = this.computeMaskImage_();
    }

    if (changedProperties.has('match') ||
        changedProperties.has('isWeatherAnswer')) {
      this.hasIconContainerBackground =
          this.computeHasIconContainerBackground_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('match') ||
        changedProperties.has('defaultIcon') ||
        changedPrivateProperties.has('isTopChromeSearchbox_')) {
      this.faviconImage_ = this.computeFaviconImage_();
    }

    if (changedProperties.has('match') ||
        changedPrivateProperties.has('faviconImage_') ||
        changedPrivateProperties.has('isTopChromeSearchbox_')) {
      this.faviconImageSrcSet_ = this.computeFaviconImageSrcSet_();
    }

    if (changedPrivateProperties.has('faviconImage_')) {
      // If `faviconImage_` changes to a new truthy value, a new favicon is
      // being loaded.
      this.faviconLoading_ = !!this.faviconImage_;
      this.faviconError_ = false;
    }

    if (changedProperties.has('match') ||
        changedPrivateProperties.has('isLensSearchbox_') ||
        changedPrivateProperties.has('faviconImage_') ||
        changedPrivateProperties.has('faviconLoading_') ||
        changedPrivateProperties.has('faviconError_')) {
      this.showFaviconImage_ = this.computeShowFaviconImage_();
    }

    if (changedPrivateProperties.has('iconSrc_')) {
      // If iconSrc_ changes to a new truthy value, a new icon is being loaded.
      this.iconLoading_ = !!this.iconSrc_;
    }

    if (changedPrivateProperties.has('imageSrc_')) {
      // If imageSrc_ changes to a new truthy value, a new image is being
      // loaded.
      this.imageLoading_ = !!this.imageSrc_;
      this.imageError_ = false;
    }

    if (changedPrivateProperties.has('imageSrc_') ||
        changedPrivateProperties.has('imageError_')) {
      this.showImage_ = this.computeShowImage_();
    }

    if (changedProperties.has('match') ||
        changedPrivateProperties.has('isLensSearchbox_') ||
        changedPrivateProperties.has('iconLoading_')) {
      this.showIconImg_ = this.computeShowIconImg_();
    }
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeFaviconUrl_(scaleFactor: number): string {
    if (!this.match?.destinationUrl.url) {
      return '';
    }

    return getFaviconUrl(
        /* url= */ this.match.destinationUrl.url, {
          forceLightMode: !this.isTopChromeSearchbox_,
          forceEmptyDefaultFavicon: true,
          scaleFactor: `${scaleFactor}x`,
        });
  }

  private computeFaviconImageSrcSet_(): string {
    if (!this.faviconImage_.startsWith('chrome://favicon2/')) {
      return '';
    }

    return [
      `${this.computeFaviconUrl_(/* scaleFactor= */ 1)} 1x`,
      `${this.computeFaviconUrl_(/* scaleFactor= */ 2)} 2x`,
    ].join(', ');
  }

  private computeFaviconImage_(): string {
    if (this.match && !this.match.isSearchType) {
      if (this.match.type === DOCUMENT_MATCH_TYPE ||
          this.match.type === PEDAL ||
          this.match.isEnterpriseSearchAggregatorPeopleType) {
        return this.match.iconPath;
      }

      // Featured enterprise search suggestions have the icon set match.iconUrl.
      if (this.match.type !== HISTORY_CLUSTER_MATCH_TYPE &&
          this.match.type !== FEATURED_ENTERPRISE_SEARCH) {
        return this.computeFaviconUrl_(/* scaleFactor= */ 1);
      }
    }

    if (this.defaultIcon ===
            '//resources/cr_components/searchbox/icons/google_g.svg' ||
        this.defaultIcon ===
            '//resources/cr_components/searchbox/icons/google_g_gradient.svg') {
      // The google_g.svg is a fully colored icon, so it needs to be displayed
      // as a favicon image as mask images will mask the colors.
      return this.defaultIcon;
    }

    return '';
  }

  private computeIsAnswer_(): boolean {
    return !!this.match && !!this.match.answer;
  }

  private computeIsWeatherAnswer_(): boolean {
    return this.match?.isWeatherAnswerSuggestion || false;
  }

  private computeHasImage_(): boolean {
    return !!this.match && !!this.match.imageUrl;
  }

  private computeIsEnterpriseSearchAggregatorPeopleType_(): boolean {
    return this.match?.isEnterpriseSearchAggregatorPeopleType || false;
  }

  private computeShowIconImg_(): boolean {
    // Lens searchbox should not use icon URL.
    return !this.isLensSearchbox_ && !!this.match && !!this.match.iconUrl.url &&
        !this.iconLoading_;
  }

  private computeMaskImage_(): string {
    // Lens searchboxes should always have the Google G in the searchbox.
    if (this.isLensSearchbox_ && this.inSearchbox) {
      return `url(${this.defaultIcon})`;
    }
    // Enterprise search aggregator people and starter pack/featured enterprise
    // search suggestions should show icon even in searchbox.
    if (this.match &&
        (!this.match.isRichSuggestion || this.match.type === STARTER_PACK ||
         this.match.type === FEATURED_ENTERPRISE_SEARCH ||
         this.match.isEnterpriseSearchAggregatorPeopleType ||
         !this.inSearchbox)) {
      return `url(${this.match.iconPath})`;
    } else {
      return `url(${this.defaultIcon})`;
    }
  }

  // Controls whether the favicon image should be used instead of the mask
  // image.
  private computeShowFaviconImage_(): boolean {
    if (!this.faviconImage_) {
      return false;
    }

    // If the favicon resource is still loading or there was an error, then
    // fall back to rendering the default vector icon (generic globe icon).
    if (this.faviconLoading_ || this.faviconError_) {
      return false;
    }

    // Navigation suggestions should always use the background image, except for
    // Lens searchboxes and pedal/starter pack suggestions, which prefer to use
    // the default icon in the mask image.
    if (!this.isLensSearchbox_ && this.match && !this.match.isSearchType &&
        this.match.type !== STARTER_PACK && this.match.type !== PEDAL) {
      return true;
    }

    // The following icons should not use the GM3 foreground color.
    // TODO(niharm): Refactor logic in C++ and send via mojom in
    // "chrome/browser/ui/webui/searchbox/searchbox_handler.cc".
    const themedIcons = [
      'calendar',
      'drive_docs',
      'drive_folder',
      'drive_form',
      'drive_image',
      'drive_logo',
      'drive_pdf',
      'drive_sheets',
      'drive_slides',
      'drive_video',
      'google_agentspace_logo',
      'google_agentspace_logo_25',
      'google_g',
      'google_g_gradient',
      'note',
      'sites',
    ];
    for (const icon of themedIcons) {
      if (this.faviconImage_ ===
          '//resources/cr_components/searchbox/icons/' + icon + '.svg') {
        return true;
      }
    }
    return false;
  }

  private computeSrc_(url: string|undefined): string {
    if (!url) {
      return '';
    }

    if (url.startsWith('data:image/')) {
      // Zero-prefix matches come with the data URI content in |url|.
      return url;
    }

    return `//image?staticEncode=true&encodeType=webp&url=${url}`;
  }

  private computeIconSrc_(): string {
    return this.computeSrc_(this.match?.iconUrl?.url);
  }

  private computeShowImage_(): boolean {
    return !!this.imageSrc_ && !this.imageError_;
  }

  private computeImageSrc_(): string {
    return this.computeSrc_(this.match?.imageUrl);
  }

  protected getContainerBgColor_(): string {
    // If the match has an image dominant color, show that color in place of the
    // image until it loads. This helps the image appear to load more smoothly.
    return ((this.imageLoading_ || this.imageError_) &&
            this.match?.imageDominantColor) ?
        // .25 opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc.
        (this.match.imageDominantColor ?
             `${this.match.imageDominantColor}40` :
             'var(--cr-searchbox-match-icon-container-background-fallback)') :
        'transparent';
  }

  protected onFaviconLoad_() {
    this.faviconLoading_ = false;
    this.faviconError_ = false;
  }

  protected onFaviconError_() {
    this.faviconLoading_ = false;
    this.faviconError_ = true;
  }

  protected onIconLoad_() {
    this.iconLoading_ = false;
  }

  protected onImageLoad_() {
    this.imageLoading_ = false;
    this.imageError_ = false;
  }

  protected onImageError_() {
    this.imageLoading_ = false;
    this.imageError_ = true;
  }

  // All pedals, starter pack/featured enterprise search suggestions, and AiS
  // except weather should have a colored background container that matches the
  // current theme.
  // TODO(niharm): Refactor logic in C++ and send via mojom in
  // "chrome/browser/ui/webui/searchbox/searchbox_handler.cc".
  private computeHasIconContainerBackground_(): boolean {
    if (this.match) {
      return this.match.type === PEDAL ||
          this.match.type === HISTORY_CLUSTER_MATCH_TYPE ||
          this.match.type === CALCULATOR || this.match.type === STARTER_PACK ||
          this.match.type === FEATURED_ENTERPRISE_SEARCH ||
          (!!this.match.answer && !this.isWeatherAnswer);
    }
    return false;
  }

  private computeIsStarterPack_(): boolean {
    return this.match?.type === STARTER_PACK;
  }

  private computeIsFeaturedEnterpriseSearch(): boolean {
    return this.match?.type === FEATURED_ENTERPRISE_SEARCH;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-icon': SearchboxIconElement;
  }
}

customElements.define(SearchboxIconElement.is, SearchboxIconElement);
