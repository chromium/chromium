// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-textarea' is a component similar to native textarea,
 * and inherits styling from cr-input.
 */
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_textarea.css.js';
import {getHtml} from './cr_textarea.html.js';

export interface CrTextareaElement {
  $: {
    firstFooter: HTMLElement,
    footerContainer: HTMLElement,
    input: HTMLTextAreaElement,
    label: HTMLElement,
    mirror: HTMLElement,
    secondFooter: HTMLElement,
    underline: HTMLElement,
  };
}

export class CrTextareaElement extends CrLitElement {
  static get is() {
    return 'cr-textarea';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether the text area should automatically get focus when the page
       * loads.
       */
      autofocus: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the text area is disabled. When disabled, the text area loses
       * focus and is not reachable by tabbing.
       */
      disabled: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the text area is required. */
      required: {
        type: Boolean,
        reflect: true,
      },

      /** Maximum length (in characters) of the text area. */
      maxlength: {type: Number},

      /**
       * Whether the text area is read only. If read-only, content cannot be
       * changed.
       */
      readonly: {
        type: Boolean,
        reflect: true,
      },

      /** Number of rows (lines) of the text area. */
      rows: {
        type: Number,
        reflect: true,
      },

      /** Caption of the text area. */
      label: {type: String},

      /**
       * Text inside the text area. If the text exceeds the bounds of the text
       * area, i.e. if it has more than |rows| lines, a scrollbar is shown by
       * default when autogrow is not set.
       */
      value: {
        type: String,
        notify: true,
      },

      internalValue_: {
        type: String,
        state: true,
      },

      /**
       * Placeholder text that is shown when no value is present.
       */
      placeholder: {type: String},

      /** Whether the textarea can auto-grow vertically or not. */
      autogrow: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Attribute to enable limiting the maximum height of a autogrow textarea.
       * Use --cr-textarea-autogrow-max-height to set the height.
       */
      hasMaxHeight: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the textarea is invalid or not. */
      invalid: {
        type: Boolean,
        reflect: true,
      },

      /**
       * First footer text below the text area. Can be used to warn user about
       * character limits.
       */
      firstFooter: {type: String},

      /**
       * Second footer text below the text area. Can be used to show current
       * character count.
       */
      secondFooter: {type: String},
    };
  }

  override autofocus: boolean = false;
  disabled: boolean = false;
  readonly: boolean = false;
  required: boolean = false;
  rows: number = 3;
  label: string = '';
  maxlength?: number;
  value: string = '';
  placeholder: string = '';
  autogrow: boolean = false;
  hasMaxHeight: boolean = false;
  invalid: boolean = false;
  firstFooter: string = '';
  secondFooter: string = '';
  protected internalValue_: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('value')) {
      // Don't allow null or undefined as these will render in the input.
      // cr-textarea cannot use Lit's "nothing" in the HTML template; this
      // breaks the underlying native textarea's auto validation if |required|
      // is set.
      this.internalValue_ =
          (this.value === undefined || this.value === null) ? '' : this.value;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('disabled')) {
      this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    }
  }

  focusInput() {
    this.$.input.focus();
  }

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   */
  protected async onInputChange_(e: Event) {
    // Ensure that |value| has been updated before re-firing 'change'.
    await this.updateComplete;
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: {sourceEvent: e}}));
  }

  protected calculateMirror_(): string {
    if (!this.autogrow) {
      return '';
    }
    // Browsers do not render empty divs. The extra space is used to render the
    // div when empty.
    const tokens = this.value ? this.value.split('\n') : [''];

    while (this.rows > 0 && tokens.length < this.rows) {
      tokens.push('');
    }
    return tokens.join('\n') + '&nbsp;';
  }

  protected onInput_(e: Event) {
    this.internalValue_ = (e.target as HTMLInputElement).value;
    this.value = this.internalValue_;
  }

  protected onInputFocusChange_() {
    // focused_ is used instead of :focus-within, so focus on elements within
    // the suffix slot does not trigger a change in input styles.
    if (this.shadowRoot!.activeElement === this.$.input) {
      this.setAttribute('focused_', '');
    } else {
      this.removeAttribute('focused_');
    }
  }

  protected getFooterAria_(): string {
    return this.invalid ? 'assertive' : 'polite';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-textarea': CrTextareaElement;
  }
}

customElements.define(CrTextareaElement.is, CrTextareaElement);
