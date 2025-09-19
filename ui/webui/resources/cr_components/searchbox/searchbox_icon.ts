// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox_icon.css.js';
import {getHtml} from './searchbox_icon.html.js';

const CALCULATOR: string = 'search-calculator-answer';
const DOCUMENT_MATCH_TYPE: string = 'document';
const HISTORY_CLUSTER_MATCH_TYPE: string = 'history-cluster';
const PEDAL: string = 'pedal';

export interface SearchboxIconElement {
  $: {
    container: HTMLElement,
    icon: HTMLElement,
    iconImg: HTMLImageElement,
    image: HTMLImageElement,
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

      /** Used as a background image on #icon if non-empty. */
      backgroundImage: {
        type: String,
        reflect: true,
      },

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

      /** Used as a mask image on #icon if |backgroundImage| is empty. */
      maskImage: {
        type: String,
        reflect: true,
      },

      match: {type: Object},

      //========================================================================
      // Private properties
      //========================================================================

      iconStyle_: {type: String},
      iconSrc_: {type: String},

      /**
       * Flag indicating whether or not an icon image is loading. This is used
       * to show a default icon while the image is loading.
       */
      iconLoading_: {type: Boolean},

      /**
       * Whether to use the icon image instead of the default icon for the
       * suggestion.
       */
      showIconImg_: {type: Boolean},

      imageSrc_: {type: String},

      /**
       * Flag indicating whether or not an image is loading. This is used to
       * show a placeholder color while the image is loading.
       */
      imageLoading_: {type: Boolean},

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor backgroundImage: string;
  accessor defaultIcon: string = '';
  accessor hasIconContainerBackground: boolean;
  accessor inSearchbox: boolean = false;
  accessor isAnswer: boolean;
  accessor isWeatherAnswer: boolean;
  accessor isEnterpriseSearchAggregatorPeopleType: boolean;
  accessor maskImage: string;
  accessor match: AutocompleteMatch;
  protected accessor iconStyle_: string;
  protected accessor iconSrc_: string;
  private accessor iconLoading_: boolean = false;
  protected accessor showIconImg_: boolean;
  protected accessor imageSrc_: string;
  private accessor imageLoading_: boolean = false;
  private accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('match')) {
      this.backgroundImage = this.computeBackgroundImage_();
      this.iconSrc_ = this.computeIconSrc_();
      this.imageSrc_ = this.computeImageSrc_();
      this.isAnswer = this.computeIsAnswer_();
      this.isEnterpriseSearchAggregatorPeopleType =
          this.computeIsEnterpriseSearchAggregatorPeopleType_();
      this.isWeatherAnswer = this.computeIsWeatherAnswer_();
      this.maskImage = this.computeMaskImage_();
    }

    if (changedProperties.has('match') ||
        changedProperties.has('isWeatherAnswer')) {
      this.hasIconContainerBackground =
          this.computeHasIconContainerBackground_();
    }

    if (changedProperties.has('backgroundImage') ||
        changedProperties.has('maskImage')) {
      this.iconStyle_ = this.computeIconStyle_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('iconSrc_')) {
      // If iconSrc_ changes to a new truthy value, a new icon is being loaded.
      this.iconLoading_ = !!this.iconSrc_;
    }

    if (changedPrivateProperties.has('imageSrc_')) {
      // If imageSrc_ changes to a new truthy value, a new image is being
      // loaded.
      this.imageLoading_ = !!this.imageSrc_;
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

  private computeBackgroundImage_(): string {
    if (this.match && !this.match.isSearchType) {
      if (this.match.type === DOCUMENT_MATCH_TYPE ||
          this.match.type === PEDAL ||
          this.match.isEnterpriseSearchAggregatorPeopleType) {
        return `url(${this.match.iconPath})`;
      }

      if (this.match.type !== HISTORY_CLUSTER_MATCH_TYPE) {
        return getFaviconForPageURL(
            this.match.destinationUrl.url, /* isSyncedUrlForHistoryUi= */ false,
            /* remoteIconUrlForUma= */ '', /* size= */ 16,
            /* forceLightMode= */ true);
      }
    }

    if (this.defaultIcon ===
            '//resources/cr_components/searchbox/icons/google_g.svg' ||
        this.defaultIcon ===
            '//resources/cr_components/searchbox/icons/google_g_gradient.svg') {
      // The google_g.svg is a fully colored icon, so it needs to be displayed
      // as a background image as mask images will mask the colors.
      return `url(${this.defaultIcon})`;
    }

    return '';
  }

  private computeIsAnswer_(): boolean {
    return this.match && !!this.match.answer;
  }

  private computeIsWeatherAnswer_(): boolean {
    return this.match?.isWeatherAnswerSuggestion || false;
  }

  private computeIsEnterpriseSearchAggregatorPeopleType_(): boolean {
    return this.match?.isEnterpriseSearchAggregatorPeopleType || false;
  }

  private computeShowIconImg_(): boolean {
    // Lens searchbox should not use icon URL.
    return !this.isLensSearchbox_ && this.match && !!this.match.iconUrl.url &&
        !this.iconLoading_;
  }

  private computeMaskImage_(): string {
    // Lens searchboxes should always have the Google G in the searchbox.
    if (this.isLensSearchbox_ && this.inSearchbox) {
      return `url(${this.defaultIcon})`;
    }
    // Enterprise search aggregator people suggestions should show icon even in
    // searchbox.
    if (this.match &&
        (!this.match.isRichSuggestion ||
         this.match.isEnterpriseSearchAggregatorPeopleType ||
         !this.inSearchbox)) {
      return `url(${this.match.iconPath})`;
    } else {
      return `url(${this.defaultIcon})`;
    }
  }

  private computeIconStyle_(): string {
    if (this.showBackgroundImage_()) {
      return `background-image: ${this.backgroundImage};` +
          `background-color: transparent;`;
    } else {
      return `-webkit-mask-image: ${this.maskImage};`;
    }
  }

  // The following icons should not use the GM3 foreground color
  // TODO(niharm): Refactor logic in C++ and send via mojom in
  // "chrome/browser/ui/webui/searchbox/searchbox_handler.cc".
  private showBackgroundImage_(): boolean {
    const imageUrl = this.backgroundImage;
    if (!imageUrl) {
      return false;
    }
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
      'google_g',
      'google_g_gradient',
      'note',
      'sites',
    ];
    for (const icon of themedIcons) {
      if (imageUrl ===
          'url(//resources/cr_components/searchbox/icons/' + icon + '.svg)') {
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

  private computeImageSrc_(): string {
    return this.computeSrc_(this.match?.imageUrl);
  }

  protected getContainerBgColor_(): string {
    // If the match has an image dominant color, show that color in place of the
    // image until it loads. This helps the image appear to load more smoothly.
    return (this.imageLoading_ && this.match.imageDominantColor) ?
        // .25 opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc.
        `${this.match.imageDominantColor}40` :
        'transparent';
  }

  protected onIconLoad_() {
    this.iconLoading_ = false;
  }

  protected onImageLoad_() {
    this.imageLoading_ = false;
  }

  // All pedals and AiS except weather should be have a background that
  // matches theme.
  // TODO(niharm): Refactor logic in C++ and send via mojom in
  // "chrome/browser/ui/webui/searchbox/searchbox_handler.cc".
  private computeHasIconContainerBackground_(): boolean {
    if (this.match) {
      return this.match.type === PEDAL ||
          this.match.type === HISTORY_CLUSTER_MATCH_TYPE ||
          this.match.type === CALCULATOR ||
          (!!this.match.answer && !this.isWeatherAnswer);
    }
    return false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-icon': SearchboxIconElement;
  }
}

customElements.define(SearchboxIconElement.is, SearchboxIconElement);
