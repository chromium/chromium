// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './searchbox_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxIconElement} from './searchbox_icon.js';
import {getCss} from './searchbox_input.css.js';
import {getHtml} from './searchbox_input.html.js';

const MULTILINE_INPUT_HEIGHT_THRESHOLD = 48;

export interface Input {
  text: string;
  inline: string;
}

export interface InputUpdate {
  text?: string;
  inline?: string;
  moveCursorToEnd?: boolean;
}

const SearchboxInputElementBase = I18nMixinLit(CrLitElement);

export interface SearchboxInputElement {
  $: {
    input: HTMLInputElement|HTMLTextAreaElement,
    icon: SearchboxIconElement,
  };
}

export class SearchboxInputElement extends SearchboxInputElementBase {
  static get is() {
    return 'cr-searchbox-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dropdownIsVisible: {type: Boolean, reflect: true},
      inputAriaLive: {type: String},
      multiLineEnabled: {type: Boolean, reflect: true},
      placeholderText: {type: String},
      searchboxAriaDescription: {type: String},
      searchboxIcon: {type: String},
      selectedMatch: {type: Object},
      inputHasMatches: {type: Boolean},
      allowFilePaste: {type: Boolean},
    };
  }

  accessor dropdownIsVisible: boolean = false;
  accessor inputAriaLive: string = '';
  accessor multiLineEnabled: boolean = false;
  accessor placeholderText: string = '';
  accessor searchboxAriaDescription: string = '';
  accessor searchboxIcon: string = '';
  accessor selectedMatch: AutocompleteMatch|null = null;
  accessor inputHasMatches: boolean = false;
  accessor allowFilePaste: boolean = false;

  private callbackRouter_: PageCallbackRouter;
  private inputTextChangedListenerId_: number|null = null;
  private lastInput_: Input = {text: '', inline: ''};
  private isDeletingInput_: boolean = false;
  private pastedInInput_: boolean = false;

  constructor() {
    super();
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.inputTextChangedListenerId_ =
        this.callbackRouter_.setInputText.addListener(
            this.onSetInputText_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.inputTextChangedListenerId_);
    this.callbackRouter_.removeListener(this.inputTextChangedListenerId_);
  }

  get inputElement(): HTMLInputElement|HTMLTextAreaElement {
    assert(this.$.input);
    return this.$.input;
  }

  override focus() {
    assert(this.$.input);
    this.$.input.focus();
  }

  override blur() {
    assert(this.$.input);
    this.$.input.blur();
  }

  select() {
    assert(this.$.input);
    this.$.input.select();
  }

  setSelectionRange(
      start: number|null, end: number|null,
      direction?: 'forward'|'backward'|'none') {
    assert(this.$.input);
    this.$.input.setSelectionRange(start, end, direction);
  }

  getInputValue(): string {
    assert(this.$.input);
    return this.$.input.value;
  }

  setInputText(text: string) {
    this.onSetInputText_(text);
  }

  setInput(update: InputUpdate) {
    this.updateInput_(update);
  }

  lastInput(): Input|null {
    return this.lastInput_;
  }

  isMultiline(): boolean {
    if (!this.$.input) {
      return false;
    }
    return this.multiLineEnabled &&
        this.$.input.scrollHeight > MULTILINE_INPUT_HEIGHT_THRESHOLD;
  }

  preventInlineAutocomplete(input: string) {
    const caretNotAtEnd =
        this.$.input ? this.$.input.selectionStart !== input.length : false;
    return this.isDeletingInput_ || this.pastedInInput_ || caretNotAtEnd;
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private onSetInputText_(inputText: string) {
    this.updateInput_({text: inputText, inline: ''});
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onInputCopy_(e: ClipboardEvent) {
    this.onInputCutCopy_(e);
  }

  protected onInputCut_(e: ClipboardEvent) {
    this.onInputCutCopy_(e);
  }

  private onInputCutCopy_(e: ClipboardEvent) {
    // Only handle cut/copy when input has content and it's all selected.
    if (!this.$.input.value || this.$.input.selectionStart !== 0 ||
        this.$.input.selectionEnd !== this.$.input.value.length ||
        !this.inputHasMatches) {
      return;
    }

    if (this.selectedMatch && !this.selectedMatch.isSearchType) {
      e.clipboardData!.setData('text/plain', this.selectedMatch.destinationUrl);
      e.preventDefault();
      if (e.type === 'cut') {
        this.updateInput_({text: '', inline: ''});
        this.fire('searchbox-input-text-updated', {
          value: '',
          isComposing: false,
        });
      }
    }
  }

  protected onInputInput_(e: InputEvent) {
    const inputValue = this.$.input.value;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    if (lastInputValue === inputValue) {
      return;
    }

    this.updateInput_({text: inputValue, inline: ''});
    this.fire('searchbox-input-text-updated', {
      value: inputValue,
      isComposing: e.isComposing,
    });

    // If a character has been typed, mark 'CharTyped'. Otherwise clear it. If
    // 'CharTyped' mark already exists, there's a pending typed character for
    // which the results have not been painted yet. In that case, keep the
    // earlier mark.
    if (loadTimeData.getBoolean('reportMetrics')) {
      const charTyped = !this.isDeletingInput_ && !!inputValue.trim();
      const metricsReporter = MetricsReporterImpl.getInstance();
      if (charTyped) {
        if (!metricsReporter.hasLocalMark('CharTyped')) {
          metricsReporter.mark('CharTyped');
        }
      } else {
        metricsReporter.clearMark('CharTyped');
      }
    }

    this.pastedInInput_ = false;
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    // Ignore this event if the input does not have any inline autocompletion.
    if (!this.lastInput_.inline) {
      return;
    }

    const inputValue = this.$.input.value;
    const inputSelection = inputValue.substring(
        this.$.input.selectionStart!, this.$.input.selectionEnd!);
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;
    // If the current input state (its value and selection) matches its last
    // state (text and inline autocompletion) and the user types the next
    // character in the inline autocompletion, stop the keydown event. Just move
    // the selection. This is needed to avoid flicker.
    if (inputSelection === this.lastInput_.inline &&
        inputValue === lastInputValue &&
        this.lastInput_.inline[0]!.toLocaleLowerCase() ===
            e.key.toLocaleLowerCase()) {
      const text = this.lastInput_.text + e.key;
      assert(text);
      this.updateInput_({
        text: text,
        inline: this.lastInput_.inline.substr(1),
      });
      this.fire('searchbox-input-text-updated', {
        value: this.lastInput_.text,
        isComposing: false,
      });

      // If 'CharTyped' mark already exists, there's a pending typed character
      // for which the results have not been painted yet. In that case, keep the
      // earlier mark.
      if (loadTimeData.getBoolean('reportMetrics')) {
        const metricsReporter = MetricsReporterImpl.getInstance();
        if (!metricsReporter.hasLocalMark('CharTyped')) {
          metricsReporter.mark('CharTyped');
        }
      }
      e.preventDefault();
    }
  }

  protected onInputKeyup_(e: KeyboardEvent) {
    if (e.key !== 'Tab') {
      return;
    }

    // User is tabbing into the input element.
    this.fire(
        'searchbox-input-tab-or-mouse-clicked', {value: this.$.input.value});
  }

  protected onInputMousedown_(e: MouseEvent|null) {
    // Non-main (generally left) mouse clicks are ignored.
    if (e && e.button !== 0) {
      return;
    }

    this.fire(
        'searchbox-input-tab-or-mouse-clicked', {value: this.$.input.value});
  }

  protected onInputPaste_(e: ClipboardEvent) {
    if (this.allowFilePaste && e.clipboardData?.files &&
        e.clipboardData.files.length > 0) {
      e.preventDefault();
      this.fire('searchbox-input-files-pasted', {
        files: e.clipboardData.files,
      });
      return;
    }
    this.pastedInInput_ = true;
  }

  /**
   * Updates the input state (text and inline autocompletion) with |update|.
   */
  private updateInput_(update: InputUpdate) {
    const newInput = Object.assign({}, this.lastInput_, update);
    const newInputValue = newInput.text + newInput.inline;
    const lastInputValue = this.lastInput_.text + this.lastInput_.inline;

    const inlineDiffers = newInput.inline !== this.lastInput_.inline;
    const preserveSelection = !inlineDiffers && !update.moveCursorToEnd;
    let needsSelectionUpdate = !preserveSelection;

    const oldSelectionStart = this.$.input?.selectionStart || null;
    const oldSelectionEnd = this.$.input?.selectionEnd || null;

    if (this.$.input && newInputValue !== this.$.input.value) {
      this.$.input.value = newInputValue;
      needsSelectionUpdate = true;  // Setting .value blows away selection.
    }

    if (this.$.input && newInputValue.trim() && needsSelectionUpdate) {
      // If the cursor is to be moved to the end (implies selection should not
      // be preserved), set the selection start to same as the selection end.
      this.$.input.selectionStart = preserveSelection ? oldSelectionStart :
          update.moveCursorToEnd                      ? newInputValue.length :
                                                        newInput.text.length;
      this.$.input.selectionEnd =
          preserveSelection ? oldSelectionEnd : newInputValue.length;
    }

    this.isDeletingInput_ = lastInputValue.length > newInputValue.length &&
        lastInputValue.startsWith(newInputValue);
    this.lastInput_ = newInput;
  }

  protected computePlaceholderText_(): string {
    return this.placeholderText || this.i18n('searchBoxHint');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-input': SearchboxInputElement;
  }
}

customElements.define(SearchboxInputElement.is, SearchboxInputElement);
