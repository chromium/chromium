// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_compose_button.js';
import './searchbox_dropdown.js';
import './searchbox_icon.js';
import './searchbox_thumbnail.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/recent_tab_chip.js';
import '//resources/cr_components/search/animated_glow.js';
import './searchbox_input.js';

import type {ContextualUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import {GlifAnimationState, recordContextAdditionMethod} from '//resources/cr_components/composebox/common.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {RecentTabChipElement} from '//resources/cr_components/composebox/recent_tab_chip.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {isMac} from '//resources/js/platform.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from '//resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './searchbox.css.js';
import {getHtml} from './searchbox.html.js';
import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from './searchbox_dropdown.js';
import type {SearchboxInputElement} from './searchbox_input.js';
import {waitForLazyRender} from './utils.js';

// LINT.IfChange(GhostLoaderTagName)
const LENS_GHOST_LOADER_TAG_NAME = 'cr-searchbox-ghost-loader';
// LINT.ThenChange(/chrome/browser/resources/lens/shared/searchbox_ghost_loader.ts:GhostLoaderTagName)
// The NTP Realbox entry point is always part of the Next experience, so log
// the source value with the "crn" component.
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE = 'chrome.crn.rb';
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE = '42';

// Register --placeholder-opacity as type <number> so that we can animate it.
CSS.registerProperty({
  name: '--placeholder-opacity',
  syntax: '<number>',
  initialValue: '1',
  inherits: true,
});

import {PlaceholderTextCycler} from './placeholder_text_cycler.js';

interface ClickEventDetail {
  button: number;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
}

export interface OpenComposeboxEventDetail {
  searchboxText: string;
  contextFiles: ContextualUpload[];
  mode: ToolMode;
  model: ModelMode;
}

export interface SearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const SearchboxElementBase = I18nMixinLit(WebUiListenerMixinLit(CrLitElement));

/** A real search box that behaves just like the Omnibox. */
export class SearchboxElement extends SearchboxElementBase implements
    DragAndDropHost {
  static get is() {
    return 'cr-searchbox';
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
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      colorSourceIsBaseline: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the cr-searchbox-dropdown should be visible. */
      dropdownIsVisible: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflect: true,
      },

      multiLineEnabled: {
        type: Boolean,
        reflect: true,
      },

      /** The aria description to include on the input element. */
      searchboxAriaDescription: {type: String},

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled: {
        type: Boolean,
        reflect: true,
      },

      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },

      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },

      ntpRealboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },

      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },

      cyclingPlaceholders: {
        type: Boolean,
      },

      composeboxEnabled: {type: Boolean},

      composeButtonEnabled: {type: Boolean},

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      inputFocused_: {
        type: Boolean,
        reflect: true,
      },

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      enableThumbnailSizingTweaks_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether user is deleting text in the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      isDeletingInput_: {type: Boolean},

      /**
       * The 'Enter' keydown event that was ignored due to matches being stale.
       * Used to navigate to the default match once up-to-date matches arrive.
       */
      lastIgnoredEnterEvent_: {type: Object},

      /**
       * Last state of the input (text and inline autocompletion). Updated
       * by the user input or by the currently selected autocomplete match.
       */
      lastInput_: {type: Object},

      /** The last queried input text. */
      lastQueriedInput_: {type: String},

      /**
       * True if user just pasted into the input. Used to prevent the default
       * match from offering inline autocompletion.
       */
      pastedInInput_: {type: Boolean},

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      result_: {type: Object},

      /** The currently selected match, if any. */
      selectedMatch_: {type: Object},

      /**
       * Index of the currently selected match, if any.
       * Do not modify this. Use <cr-searchbox-dropdown> API to change
       * selection.
       */
      selectedMatchIndex_: {type: Number},

      showThumbnail: {
        type: Boolean,
        reflect: true,
      },

      thumbnailUrl_: {type: String},
      isThumbnailDeletable_: {type: Boolean},

      /** The value of the input element's 'aria-live' attribute. */
      inputAriaLive_: {type: String},

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      tabSuggestions_: {type: Array},
      recentTabForChip_: {type: Object},
      inputState_: {type: Object},
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      animationState: {
        reflect: true,
        type: String,
      },
      showModelPicker_: {
        type: Boolean,
      },
    };
  }

  accessor canShowSecondarySide: boolean = false;
  accessor colorSourceIsBaseline: boolean = false;
  accessor dropdownIsVisible: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor isDark: boolean = false;
  accessor multiLineEnabled: boolean = false;
  accessor searchboxAriaDescription: string = '';
  accessor searchboxLensSearchEnabled: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor searchboxLayoutMode: string = '';
  accessor ntpRealboxNextEnabled: boolean = false;
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor cyclingPlaceholders: boolean = false;
  accessor composeboxEnabled: boolean = false;
  accessor composeButtonEnabled: boolean = false;
  accessor showThumbnail: boolean = false;
  accessor placeholderText: string = '';
  accessor isDraggingFile: boolean = false;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  protected accessor inputAriaLive_: string = '';
  protected accessor inputFocused_: boolean = false;
  protected accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');
  protected accessor enableThumbnailSizingTweaks_: boolean =
      loadTimeData.getBoolean('enableThumbnailSizingTweaks');
  private accessor lastIgnoredEnterEvent_: KeyboardEvent|null = null;
  private accessor lastQueriedInput_: string|null = null;
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor showModelPicker_: boolean =
      loadTimeData.valueExists('contextualMenuUsePecApi') ?
      loadTimeData.getBoolean('contextualMenuUsePecApi') :
      false;
  protected accessor result_: AutocompleteResult|null = null;
  protected accessor selectedMatch_: AutocompleteMatch|null = null;
  protected accessor selectedMatchIndex_: number = -1;
  protected accessor thumbnailUrl_: string = '';
  protected accessor isThumbnailDeletable_: boolean = false;
  private accessor useWebkitSearchIcons_: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor recentTabForChip_: TabInfo|null = null;
  protected accessor inputState_: InputState|null = null;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  protected dragAndDropHandler: DragAndDropHandler|null = null;
  private dragAndDropEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
  private autocompleteResultChangedListenerId_: number|null = null;
  private thumbnailChangedListenerId_: number|null = null;
  private onTabStripChangedListenerId_: number|null = null;
  private placeholderCycler_: PlaceholderTextCycler|null = null;
  private contextMenuOpened_: boolean = false;
  private initialInputScrollHeight_: number = 0;

  constructor() {
    performance.mark('realbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged_.bind(this));
    this.thumbnailChangedListenerId_ =
        this.callbackRouter_.setThumbnail.addListener(
            this.onSetThumbnail_.bind(this));
    this.onTabStripChangedListenerId_ =
        this.callbackRouter_.onTabStripChanged.addListener(
            this.refreshTabSuggestions_.bind(this));
    this.inputState_ = (await this.pageHandler_.getInputState()).state;
    if (this.inputState_) {
      this.inputState_.activeModel = ModelMode.kUnspecified;
    }

    if (this.ntpRealboxNextEnabled) {
      this.dragAndDropHandler =
          new DragAndDropHandler(this, this.dragAndDropEnabled_);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    assert(this.thumbnailChangedListenerId_);
    this.callbackRouter_.removeListener(this.thumbnailChangedListenerId_);
    assert(this.onTabStripChangedListenerId_);
    this.callbackRouter_.removeListener(this.onTabStripChangedListenerId_);

    this.placeholderCycler_?.stop();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('composeButtonEnabled') ||
        changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ = this.composeButtonEnabled ||
          (this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline);
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('result_') ||
        changedPrivateProperties.has('selectedMatchIndex_')) {
      this.selectedMatch_ = this.computeSelectedMatch_();
    }

    if (changedPrivateProperties.has('selectedMatch_')) {
      this.inputAriaLive_ = this.computeInputAriaLive_();
    }

    if (changedPrivateProperties.has('thumbnailUrl_')) {
      this.showThumbnail = !!this.thumbnailUrl_;
    }

    if (changedPrivateProperties.has('tabSuggestions_')) {
      this.recentTabForChip_ =
          this.tabSuggestions_.find(tab => tab.showInCurrentTabChip) || null;
      if (!this.recentTabForChip_) {
        this.recentTabForChip_ =
            this.tabSuggestions_.find(tab => tab.showInPreviousTabChip) || null;
      }
    }
  }

  override firstUpdated() {
    performance.measure('realbox-creation', 'realbox-creation-start');
    if (this.multiLineEnabled) {
      this.initialInputScrollHeight_ = this.$.input.scrollHeight;
    }

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler_.getPlaceholderConfig();
        const texts = config.texts;
        assert(texts[0]);
        this.placeholderText = texts[0];
        this.placeholderCycler_ = new PlaceholderTextCycler(
            this.$.input.inputElement, texts,
            Number(config.changeTextAnimationInterval.microseconds / 1000n),
            Number(config.fadeTextAnimationDuration.microseconds / 1000n));
        this.placeholderCycler_.start();
      });
    }
  }

  private computeInputAriaLive_(): string {
    return this.selectedMatch_ ? 'off' : 'polite';
  }

  getSuggestionsElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  getDropTarget() {
    return this;
  }

  addDroppedFiles(files: FileList) {
    this.processFiles_(files, ComposeboxContextAddedMethod.DRAG_AND_DROP);
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    if (!isEnabled) {
      return false;
    }

    if (!this.isInputEmpty()) {
      return false;
    }

    if (this.dropdownIsVisible &&
        (this.composeButtonEnabled ||
         this.searchboxLayoutMode.startsWith('Tall'))) {
      return false;
    }

    return true;
  }

  queryAutocomplete() {
    // Query autocomplete if dropdown is not visible
    if (this.dropdownIsVisible) {
      return;
    }
    this.queryAutocomplete_(this.$.input.inputElement.value);
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  focusInput() {
    this.$.input.focus();
  }

  selectAll() {
    this.$.input.select();
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private async onAutocompleteResultChanged_(result: AutocompleteResult) {
    if (this.lastQueriedInput_ === null ||
        this.lastQueriedInput_.trimStart() !== result.input) {
      return;  // Stale result; ignore.
    }

    this.result_ = result;
    const hasMatches = result.matches?.length > 0;
    const hasPrimaryMatches = result.matches?.some(match => {
      const sideType =
          result.suggestionGroupsMap[match.suggestionGroupId]?.sideType ||
          SideType.kDefaultPrimary;
      return sideType === SideType.kDefaultPrimary;
    });

    this.dropdownIsVisible = hasPrimaryMatches;

    // In multi-line mode, suppress the dropdown when text wraps
    // or when the only match is the mirror query.
    if (this.multiLineEnabled && this.dropdownIsVisible) {
      const isUserTyping = result.input.trim().length > 0;
      if (isUserTyping &&
          this.shouldSuppressDropdownForMultiline_(result.matches?.length || 0)) {
        this.dropdownIsVisible = false;
      }
    }

    const firstMatch = hasMatches ? this.result_.matches[0] : null;
    if (firstMatch && firstMatch.allowedToBeDefaultMatch) {
      // Select the default match and update the input.
      this.getSuggestionsElement().selectFirst();
      this.$.input.setInput({
        text: this.lastQueriedInput_,
        inline: firstMatch.inlineAutocompletion,
      });

      // Navigate to the default up-to-date match if the user typed and pressed
      // 'Enter' too fast.
      if (this.lastIgnoredEnterEvent_) {
        this.navigateToMatch_(0, this.lastIgnoredEnterEvent_);
        this.lastIgnoredEnterEvent_ = null;
      }
    } else if (
        this.$.input.inputElement.value.trim() && hasMatches &&
        this.selectedMatchIndex_ >= 0 &&
        this.selectedMatchIndex_ < this.result_.matches.length) {
      // Restore the selection and update the input. Don't restore when the
      // user deletes all their input and autocomplete is queried or else the
      // empty input will change to the value of the first result.
      await this.getSuggestionsElement().selectIndex(this.selectedMatchIndex_);
      this.$.input.setInput({
        text: this.selectedMatch_!.fillIntoEdit,
        inline: '',
        moveCursorToEnd: true,
      });
    } else {
      // Remove the selection and update the input.
      this.getSuggestionsElement().unselect();
      this.$.input.setInput({
        inline: '',
      });
    }
  }

  private onSetThumbnail_(thumbnailUrl: string, isDeletable: boolean) {
    this.thumbnailUrl_ = thumbnailUrl;
    this.isThumbnailDeletable_ = isDeletable;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onInputFocus_() {
    this.inputFocused_ = true;
    this.pageHandler_.onFocusChanged(true);
    this.placeholderCycler_?.stop();
    if (this.ntpRealboxNextEnabled) {
      // Refresh tab suggestions to ensure the recent tab chip is up to date.
      this.refreshTabSuggestions_(/*forceRefresh=*/ true);
    }
  }

  protected onInputFocusout_() {
    this.inputFocused_ = false;
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    // For lens searchboxes, requery autcomplete for all updates to the input
    // (even if the input is empty).
    if (e.detail.value.trim() || this.isLensSearchbox_) {
      // TODO(crbug.com/40732045): Rather than disabling inline autocompletion
      // when the input event is fired within a composition session, change the
      // mechanism via which inline autocompletion is shown in the searchbox.
      this.queryAutocomplete_(e.detail.value, e.detail.isComposing);
    } else {
      this.clearAutocompleteMatches_();
    }
  }

  protected onSearchboxInputTabOrMouseClicked_(
      e: CustomEvent<{value: string}>) {
    this.inputFocused_ = true;
    if (this.dropdownIsVisible) {
      return;
    }
    this.queryAutocomplete_(e.detail.value);
  }

  protected onSearchboxInputFilesPasted_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(e.detail.files, ComposeboxContextAddedMethod.COPY_PASTE);
  }

  protected onInputWrapperFocusout_(e: FocusEvent) {
    const newlyFocusedEl = e.relatedTarget as Element;
    // Hide the matches and stop autocomplete only when the focus goes outside
    // of the searchbox wrapper. If focus is still in the searchbox wrapper,
    // exit early.
    if (this.$.inputWrapper.contains(newlyFocusedEl)) {
      return;
    }

    // If this is a Lens searchbox, treat the ghost loader as keeping searchbox
    // focus.
    // TODO(380467089): This workaround wouldn't be needed if the ghost loader
    // was part of the searchbox element. Remove this workaround once they are
    // combined.
    if (this.isLensSearchbox_ &&
        newlyFocusedEl?.tagName.toLowerCase() === LENS_GHOST_LOADER_TAG_NAME) {
      return;
    }

    if (this.lastQueriedInput_ === '') {
      // Clear the input as well as the matches if the input was empty when
      // the matches arrived.
      this.$.input.setInput({text: '', inline: ''});
      this.clearAutocompleteMatches_();
    } else {
      this.dropdownIsVisible = false;

      // Stop autocomplete but leave (potentially stale) results and continue
      // listening for key presses. These stale results should never be shown.
      // They correspond to the potentially stale suggestion left in the
      // searchbox when blurred. That stale result may be navigated to by
      // focusing and pressing 'Enter'.
      this.pageHandler_.stopAutocomplete(/*clearResult=*/ false);
    }
    this.pageHandler_.onFocusChanged(false);
    this.placeholderCycler_?.start();
  }

  protected async onInputWrapperKeydown_(e: KeyboardEvent) {
    const modifier = isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
    if (modifier && e.key === 'z') {
      e.stopPropagation();
      return;
    }

    const KEYDOWN_HANDLED_KEYS = [
      'ArrowDown',
      'ArrowUp',
      'Backspace',
      'Delete',
      'Enter',
      'Escape',
      'PageDown',
      'PageUp',
      'Tab',
    ];
    if (!KEYDOWN_HANDLED_KEYS.includes(e.key)) {
      return;
    }

    if (e.defaultPrevented) {
      // Ignore previously handled events.
      return;
    }

    if (this.showThumbnail) {
      const thumbnail =
          this.shadowRoot.querySelector<HTMLElement>('cr-searchbox-thumbnail');
      if (thumbnail === this.shadowRoot.activeElement) {
        if (e.key === 'Backspace' || e.key === 'Enter') {
          // Remove thumbnail, focus input, and notify browser.
          this.thumbnailUrl_ = '';
          this.$.input.focus();
          this.clearAutocompleteMatches_();
          this.pageHandler_.onThumbnailRemoved();
          const inputValue = this.$.input.inputElement.value;
          // Clearing the autocomplete matches above doesn't allow for
          // navigation directly after removing the thumbnail. Must manually
          // query autocomplete after removing the thumbnail since the
          // thumbnail isn't part of the text input.
          this.queryAutocomplete_(inputValue);
          e.preventDefault();
        } else if (e.key === 'Tab' && !e.shiftKey) {
          this.$.input.focus();
          e.preventDefault();
        } else if (
            this.dropdownIsVisible &&
            (e.key === 'ArrowUp' || e.key === 'ArrowDown')) {
          // If the dropdown is visible, arrowing up and down unfocuses the
          // thumbnail and follows standard arrow up/down behavior (selects
          // the next/previous match).
          this.$.input.focus();
        }
      } else if (
          this.isThumbnailDeletable_ &&
          this.$.input.inputElement.selectionStart === 0 &&
          this.$.input.inputElement.selectionEnd === 0 &&
          this.$.input === this.shadowRoot.activeElement &&
          (e.key === 'Backspace' || (e.key === 'Tab' && e.shiftKey))) {
        // Backspacing or shift-tabbing the thumbnail results in the thumbnail
        // being focused.
        thumbnail?.focus();
        e.preventDefault();
      }
    }

    if (this.composeButtonEnabled && e.key === 'Tab' &&
        this.$.input?.lastInput()?.inline &&
        this.$.input === this.shadowRoot.activeElement) {
      if (e.shiftKey) {
        this.$.input.setInput({inline: ''});
        return;
      }

      const newText =
          this.$.input.lastInput()!.text + this.$.input.lastInput()!.inline;
      this.$.input.setInput({
        text: newText,
        inline: '',
        moveCursorToEnd: true,
      });
      this.queryAutocomplete_(newText);
      e.preventDefault();
      return;
    }

    if (e.key === 'Backspace' || e.key === 'Tab') {
      return;
    }


    // ArrowUp/ArrowDown query autocomplete when matches are not visible.
    if (!this.dropdownIsVisible) {
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        if (this.multiLineEnabled &&
            this.shouldSuppressDropdownForMultiline_(this.result_?.matches?.length || 0)) {
          return;
        }
        const inputValue = this.$.input.inputElement.value;
        if (inputValue.trim() || !inputValue) {
          this.queryAutocomplete_(inputValue);
        }
        e.preventDefault();
        return;
      }
    }

    if (e.key === 'Escape') {
      this.fire('escape-searchbox', {
        event: e,
        emptyInput: !this.$.input.inputElement.value,
      });
    }

    // Do not handle the following keys if there are no matches available.
    if (!this.result_ || this.result_.matches.length === 0) {
      return;
    }

    if (e.key === 'Delete') {
      if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
        if (this.selectedMatch_ && this.selectedMatch_.supportsDeletion) {
          this.pageHandler_.deleteAutocompleteMatch(
              this.selectedMatchIndex_, this.selectedMatch_.destinationUrl);
          e.preventDefault();
        }
      }
      return;
    }

    // Do not handle the following keys if inside an IME composition session.
    if (e.isComposing) {
      return;
    }

    if (e.key === 'Enter') {
      if (this.multiLineEnabled && e.shiftKey) {
        return;
      }
      e.preventDefault();
      const array: HTMLElement[] = [this.getSuggestionsElement(), this.$.input];
      if (!array.includes(e.target as HTMLElement)) {
        return;
      }
      const currentInput = this.result_?.input;
      const lastQueriedInput = this.lastQueriedInput_?.trimStart();
      if (currentInput !== undefined && lastQueriedInput !== undefined &&
          lastQueriedInput === currentInput) {
        if (this.selectedMatch_) {
          this.navigateToMatch_(this.selectedMatchIndex_, e);
        }
      } else {
        // User typed and pressed 'Enter' too quickly. Ignore this for now
        // because the matches are stale. Navigate to the default match (if
        // one exists) once the up-to-date matches arrive.
        this.lastIgnoredEnterEvent_ = e;
      }
      return;
    }

    // Do not handle the following keys if there are key modifiers.
    if (hasKeyModifiers(e)) {
      return;
    }

    // Clear the input as well as the matches when 'Escape' is pressed if the
    // the first match is selected or there are no selected matches.
    if (e.key === 'Escape' && this.selectedMatchIndex_ <= 0) {
      this.$.input.setInput({text: '', inline: ''});
      this.clearAutocompleteMatches_();
      e.preventDefault();
      return;
    }

    e.preventDefault();

    if (e.key === 'ArrowDown') {
      await this.getSuggestionsElement().selectNext();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, this.selectedMatch_!.destinationUrl,
          NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'ArrowUp') {
      await this.getSuggestionsElement().selectPrevious();
      this.pageHandler_.onNavigationLikely(
          this.selectedMatchIndex_, this.selectedMatch_!.destinationUrl,
          NavigationPredictor.kUpOrDownArrowButton);
    } else if (e.key === 'Escape' || e.key === 'PageUp') {
      await this.getSuggestionsElement().selectFirst();
    } else if (e.key === 'PageDown') {
      await this.getSuggestionsElement().selectLast();
    }

    // Focus the selected match if focus is currently in the matches.
    if (this.shadowRoot.activeElement === this.getSuggestionsElement()) {
      this.getSuggestionsElement().focusSelected();
    }

    // Update the input.
    const newFill = this.selectedMatch_!.fillIntoEdit;
    const newInline = this.selectedMatchIndex_ === 0 &&
            this.selectedMatch_!.allowedToBeDefaultMatch ?
        this.selectedMatch_!.inlineAutocompletion :
        '';
    const newFillEnd = newFill.length - newInline.length;
    const text = newFill.substr(0, newFillEnd);
    assert(text);
    this.$.input.setInput({
      text: text,
      inline: newInline,
      moveCursorToEnd: newInline.length === 0,
    });
  }

  /**
   * @param e Event containing index of the match that received focus.
   */
  protected async onMatchFocusin_(e: CustomEvent<number>) {
    // Select the match that received focus.
    await this.getSuggestionsElement().selectIndex(e.detail);
    // Input selection (if any) likely drops due to focus change. Simply fill
    // the input with the match and move the cursor to the end.
    this.$.input.setInput({
      text: this.selectedMatch_!.fillIntoEdit,
      inline: '',
      moveCursorToEnd: true,
    });
  }

  protected onMatchClick_() {
    this.clearAutocompleteMatches_();
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(
        e.detail.files, ComposeboxContextAddedMethod.CONTEXT_MENU);
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    const attachment: TabUpload = {
      tabId: e.detail.id,
      url: e.detail.url,
      title: e.detail.title,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    };
    this.openComposebox_([attachment]);
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    // Only refresh tab suggestions if the context menu is opened or the recent
    // tab chip is visible.
    const requiresRefresh =
        forceRefresh || this.contextMenuOpened_ || this.recentTabChipVisible_();
    if (!requiresRefresh) {
      return;
    }
    const {tabs} = await this.pageHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
  }

  protected async onGetTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.pageHandler_.getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected onContextMenuContainerClick_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();

    if (e.button !== 0 || this.inputFocused_ ||
        this.searchboxLayoutMode === 'Compact') {
      return;
    }

    this.focusInput();
    this.inputFocused_ = true;
    if (!this.dropdownIsVisible) {
      this.queryAutocomplete_(this.$.input.inputElement.value);
    }
  }

  protected onContextMenuContainerMousedown_(e: FocusEvent) {
    // Special treatment for the "Tall" layout variants where not clicking on an
    // inner element should be treated as clicking on a non-focusable area.
    if (this.searchboxLayoutMode !== 'Compact' &&
        (e.target instanceof HTMLElement &&
         e.target.id === 'contextMenuContainer')) {
      e.preventDefault();
      e.stopPropagation();
    }
  }

  protected onContextMenuClosed_() {
    this.contextMenuOpened_ = false;
    this.blur();
  }

  protected onContextMenuOpened_() {
    this.contextMenuOpened_ = true;
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onComposeClick_(e: CustomEvent<ClickEventDetail>) {
    // TODO(crbug.com/463667769): Call submitQuery here since RealboxHandler is
    // now a `ContextualSearchboxHandler`.
    this.pageHandler_.activateMetricsFunnel('AiModeButton');

    chrome.histograms.recordUserAction(
        'ContextualSearch.AiModeButtonClick.NtpRealbox');
    chrome.histograms.recordBoolean(
        'ContextualSearch.AiModeButtonClick.NtpRealbox', true);

    if (!this.composeboxEnabled || this.$.input.inputElement.value.trim()) {
      const metricName =
          'ContextualSearch.UserAction.SubmitQuery.WithoutContext.NewTabPage';
      chrome.histograms.recordUserAction(metricName);
      chrome.histograms.recordBoolean(metricName, true);

      // Construct navigation url.
      const searchParams = new URLSearchParams();
      searchParams.append('sourceid', 'chrome');
      searchParams.append('udm', '50');
      searchParams.append('aep', DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE);
      searchParams.append(
          'source', DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE);

      if (this.$.input.inputElement.value.trim()) {
        searchParams.append('q', this.$.input.inputElement.value.trim());
      }
      const queryUrl =
          new URL('/search', loadTimeData.getString('googleBaseUrl'));
      queryUrl.search = searchParams.toString();
      const href = queryUrl.href;

      // Handle mouse events.
      if (e.detail.ctrlKey || e.detail.metaKey) {
        window.open(href, '_blank');
      } else if (e.detail.shiftKey) {
        window.open(href, '_blank', 'noopener');
      } else {
        window.open(href, '_self');
      }
    } else {
      this.openComposebox_();
    }

    chrome.histograms.recordBoolean(
        'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
        !this.isInputEmpty());
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler_.activateMetricsFunnel('PlusButton');
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposebox_([], e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposebox_([], ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposebox_([], ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposebox_([], ToolMode.kUnspecified, e.detail.model);
  }

  protected openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified) {
    if (this.ntpRealboxNextEnabled) {
      const context =
          this.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
              '#context');
      assert(context);
      context.closeMenu();
    }
    this.fire<OpenComposeboxEventDetail>('open-composebox', {
      searchboxText: this.$.input.inputElement.value,
      contextFiles: uploads,
      mode: mode,
      model: model,
    });
    this.setInputText('');
  }

  hasThumbnail(): boolean {
    return !!this.thumbnailUrl_;
  }

  protected onRemoveThumbnailClick_() {
    /* Remove thumbnail, focus input, and notify browser. */
    this.thumbnailUrl_ = '';
    this.$.input.focus();
    this.clearAutocompleteMatches_();
    this.pageHandler_.onThumbnailRemoved();
    // Clearing the autocomplete matches above doesn't allow for
    // navigation directly after removing the thumbnail. Must manually
    // query autocomplete after removing the thumbnail since the
    // thumbnail isn't part of the text input.
    const inputValue = this.$.input.inputElement.value;
    this.queryAutocomplete_(inputValue);
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeSelectedMatch_(): AutocompleteMatch|null {
    if (!this.result_ || !this.result_.matches) {
      return null;
    }
    return this.result_.matches[this.selectedMatchIndex_] || null;
  }

  private shouldSuppressDropdownForMultiline_(numMatches: number): boolean {
    const inputHasWrapped = this.initialInputScrollHeight_ > 0 &&
        this.$.input.scrollHeight > this.initialInputScrollHeight_;
    return inputHasWrapped || numMatches === 1;
  }

  protected inputHasMatches_(): boolean {
    return !!this.result_ && !!this.result_.matches &&
        this.result_.matches.length > 0;
  }

  protected computeShowRecentTabChip_(): boolean {
    // composeboxShowRecentTabChip is unavailable in the WebUI Browser.
    const recentTabChipEnabled =
        loadTimeData.valueExists('composeboxShowRecentTabChip') &&
        loadTimeData.getBoolean('composeboxShowRecentTabChip');
    const isBrowserTabAllowed = !this.showModelPicker_ ||
        (!!this.inputState_ &&
         this.inputState_.allowedInputTypes.includes(InputType.kBrowserTab));
    return recentTabChipEnabled && !!this.recentTabForChip_ &&
        this.dropdownIsVisible && this.isInputEmpty() && isBrowserTabAllowed;
  }

  protected computePlaceholderText_(placeholderText: string): string {
    if (placeholderText) {
      return placeholderText;
    }
    return this.showThumbnail ? this.i18n('searchBoxHintMultimodal') :
                                this.i18n('searchBoxHint');
  }

  /**
   * Clears the autocomplete result on the page and on the autocomplete backend.
   */
  private clearAutocompleteMatches_() {
    this.dropdownIsVisible = false;
    this.result_ = null;
    this.getSuggestionsElement().unselect();
    this.pageHandler_.stopAutocomplete(/*clearResult=*/ true);
    // Autocomplete sends updates once it is stopped. Invalidate those results
    // by setting the |this.lastQueriedInput_| to its default value.
    this.lastQueriedInput_ = null;
  }

  private navigateToMatch_(matchIndex: number, e: KeyboardEvent|MouseEvent) {
    assert(matchIndex >= 0);
    const match = this.result_!.matches[matchIndex];
    assert(match);
    this.pageHandler_.openAutocompleteMatch(
        matchIndex, match.destinationUrl, this.dropdownIsVisible,
        (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
        e.shiftKey);
    this.$.input.setInput({
      text: match.fillIntoEdit,
      inline: '',
      moveCursorToEnd: true,
    });
    this.clearAutocompleteMatches_();
    e.preventDefault();
  }

  private queryAutocomplete_(
      input: string, preventInlineAutocomplete: boolean = false) {
    this.lastQueriedInput_ = input;

    preventInlineAutocomplete = preventInlineAutocomplete ||
        (this.$.input ? this.$.input.preventInlineAutocomplete(input) : false);
    this.pageHandler_.queryAutocomplete(input, preventInlineAutocomplete);

    this.fire('query-autocomplete', {inputValue: input});
  }

  private recentTabChipVisible_() {
    if (!this.ntpRealboxNextEnabled) {
      return false;
    }
    const recentTabChip = this.shadowRoot.querySelector<RecentTabChipElement>(
        'composebox-recent-tab-chip');
    return !!recentTabChip;
  }

  protected getThumbnailTabindex_(): string {
    // If the thumbnail can't be deleted, returning an empty string will set the
    // tabindex to nothing, which will make the thumbnail not focusable.
    return this.isThumbnailDeletable_ ? '1' : '';
  }

  protected onSelectedMatchIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedMatchIndex_ = e.detail.value;
  }

  protected onHadSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hadSecondarySide = e.detail.value;
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  protected useCompactLayout_(): boolean {
    return this.searchboxLayoutMode === 'Compact';
  }

  private processFiles_(
      files: FileList|null,
      contextAdditionMethod: ComposeboxContextAddedMethod) {
    if (!files || files.length === 0) {
      return;
    }
    recordContextAdditionMethod(
        contextAdditionMethod, loadTimeData.getString('composeboxSource'));
    this.openComposebox_(Array.from(files, (file) => ({file})));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox': SearchboxElement;
  }
}

customElements.define(SearchboxElement.is, SearchboxElement);
