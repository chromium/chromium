// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_input.css.js';
import {getHtml} from './cr_input.html.js';

/**
 * Input types supported by cr-input.
 */
const SUPPORTED_INPUT_TYPES: Set<string> = new Set([
  'number',
  'password',
  'search',
  'text',
  'url',
]);

/**
 * @fileoverview 'cr-input' is a component similar to native input.
 *
 * Native input attributes that are currently supported by cr-inputs are:
 *   autofocus
 *   disabled
 *   max (only applicable when type="number")
 *   min (only applicable when type="number")
 *   maxlength
 *   minlength
 *   pattern
 *   placeholder
 *   readonly
 *   required
 *   tabindex (set through input-tabindex)
 *   type (see |SUPPORTED_INPUT_TYPES| above)
 *   value
 *
 * Additional attributes that you can use with cr-input:
 *   label
 *   auto-validate - triggers validation based on |pattern| and |required|,
 *                   whenever |value| changes.
 *   error-message - message displayed under the input when |invalid| is true.
 *   invalid
 *
 * You may pass an element into cr-input via [slot="suffix"] to be vertically
 * center-aligned with the input field, regardless of position of the label and
 * error-message. Example:
 *   <cr-input>
 *     <cr-button slot="suffix"></cr-button>
 *   </cr-input>
 */
export interface CrInputElement {
  $: {
    error: HTMLElement,
    label: HTMLElement,
    input: HTMLInputElement,
    underline: HTMLElement,
  };
}

export class CrInputElement extends CrLitElement {
  static get is() {
    return 'cr-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaDescription: {type: String},
      ariaLabel: {type: String},

      autofocus: {
        type: Boolean,
        reflect: true,
      },

      autoValidate: {type: Boolean},

      disabled: {
        type: Boolean,
        reflect: true,
      },

      errorMessage: {type: String},
      errorRole_: {type: String},
      displayErrorMessage_: {type: String},

      /**
       * This is strictly used internally for styling, do not attempt to use
       * this to set focus.
       */
      focused_: {
        type: Boolean,
        reflect: true,
      },

      invalid: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      max: {
        type: Number,
        reflect: true,
      },

      min: {
        type: Number,
        reflect: true,
      },

      maxlength: {
        type: Number,
        reflect: true,
      },

      minlength: {
        type: Number,
        reflect: true,
      },

      pattern: {
        type: String,
        reflect: true,
      },

      inputmode: {type: String},
      label: {type: String},
      placeholder: {type: String},

      readonly: {
        type: Boolean,
        reflect: true,
      },

      required: {
        type: Boolean,
        reflect: true,
      },

      inputTabindex: {type: Number},
      type: {type: String},

      value: {
        type: String,
        notify: true,
      },

      internalValue_: {
        type: String,
        state: true,
      },
    };
  }

  override ariaDescription: string|null = null;
  override ariaLabel: string = '';
  override autofocus: boolean = false;
  autoValidate: boolean = false;
  disabled: boolean = false;
  errorMessage: string = '';
  inputmode?: string;
  inputTabindex: number = 0;
  invalid: boolean = false;
  label: string = '';
  max?: number;
  min?: number;
  maxlength?: number;
  minlength?: number;
  pattern?: string;
  placeholder: string|null = null;
  readonly: boolean = false;
  required: boolean = false;
  type: string = 'text';
  value: string = '';
  protected internalValue_: string = '';
  protected focused_: boolean = false;

  override firstUpdated() {
    // Use inputTabindex instead.
    assert(!this.hasAttribute('tabindex'));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('value')) {
      // Don't allow null or undefined as these will render in the input.
      // cr-input cannot use Lit's "nothing" in the HTML template; this breaks
      // the underlying native input's auto validation if |required| is set.
      this.internalValue_ =
          (this.value === undefined || this.value === null) ? '' : this.value;
    }

    if (changedProperties.has('inputTabindex')) {
      // CrInput only supports 0 or -1 values for the input's tabindex to allow
      // having the input in tab order or not. Values greater than 0 will not
      // work as the shadow root encapsulates tabindices.
      assert(this.inputTabindex === 0 || this.inputTabindex === -1);
    }

    if (changedProperties.has('type')) {
      // Check that the 'type' is one of the supported types.
      assert(SUPPORTED_INPUT_TYPES.has(this.type));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('value')) {
      const previous = changedProperties.get('value');
      if ((!!this.value || !!previous) && this.autoValidate) {
        this.invalid = !this.inputElement.checkValidity();
      }
    }

    if (changedProperties.has('placeholder')) {
      if (this.placeholder === null || this.placeholder === undefined) {
        this.inputElement.removeAttribute('placeholder');
      } else {
        this.inputElement.setAttribute('placeholder', this.placeholder);
      }
    }
  }

  get inputElement(): HTMLInputElement {
    return this.$.input;
  }

  override focus() {
    this.focusInput();
  }

  /**
   * Focuses the input element.
   * TODO(crbug.com/40593040): Replace this with focus() after resolving the text
   * selection issue described in onFocus_().
   * @return Whether the <input> element was focused.
   */
  focusInput(): boolean {
    if (this.shadowRoot!.activeElement === this.inputElement) {
      return false;
    }
    this.inputElement.focus();
    return true;
  }

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   */
  protected async onInputChange_(e: Event) {
    // Ensure that |value| has been updated before re-firing 'change'.
    await this.updateComplete;
    this.fire('change', {sourceEvent: e});
  }

  protected onInput_(e: Event) {
    this.internalValue_ = (e.target as HTMLInputElement).value;
    this.value = this.internalValue_;
  }

  protected onInputFocus_() {
    this.focused_ = true;
  }

  protected onInputBlur_() {
    this.focused_ = false;
  }

  protected getAriaLabel_() {
    return this.ariaLabel || this.label || this.placeholder;
  }

  protected getAriaInvalid_() {
    return this.invalid ? 'true' : 'false';
  }

  protected getErrorMessage_() {
    return this.invalid ? this.errorMessage : '';
  }

  protected getErrorRole_() {
    // On VoiceOver role="alert" is not consistently announced when its
    // content changes. Adding and removing the |role| attribute every time
    // there is an error, triggers VoiceOver to consistently announce.
    return this.invalid ? 'alert' : '';
  }

  protected getAriaErrorMessage_() {
    return this.invalid ? 'error' : '';
  }

  /**
   * Selects the text within the input. If no parameters are passed, it will
   * select the entire string. Either no params or both params should be passed.
   * Publicly, this function should be used instead of inputElement.select() or
   * manipulating inputElement.selectionStart/selectionEnd because the order of
   * execution between focus() and select() is sensitive.
   */
  select(start?: number, end?: number) {
    this.inputElement.focus();
    if (start !== undefined && end !== undefined) {
      this.inputElement.setSelectionRange(start, end);
    } else {
      // Can't just pass one param.
      assert(start === undefined && end === undefined);
      this.inputElement.select();
    }
  }

  // Note: In order to preserve it as a synchronous API, validate() forces 2
  // rendering updates to cr-input. This allows this function to be used to
  // synchronously determine the validity of a <cr-input>, however, as a result
  // of these 2 forced updates it may result in slower performance. validate()
  // should not be called internally from within cr_input.ts, and should only
  // be called where necessary from clients.
  validate(): boolean {
    // Ensure that any changes to |value| have propagated to the native <input>.
    this.performUpdate();
    this.invalid = !this.inputElement.checkValidity();
    // Perform update again to ensure change propagates via 2 way binding to
    // Polymer parent before returning.
    this.performUpdate();
    return !this.invalid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-input': CrInputElement;
  }
}

customElements.define(CrInputElement.is, CrInputElement);
