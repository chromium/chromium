// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '../hidden_style_css.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import './cr_input_style.css.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '../../js/assert.m.js';


/**
 * Input types supported by cr-input.
 * @type {!Set<string>}
 */
const SUPPORTED_INPUT_TYPES = new Set([
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
Polymer({
  is: 'cr-input',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {string|undefined} */
    ariaDescription: {
      type: String,
    },

    ariaLabel: {
      type: String,
      value: '',
    },

    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    autoValidate: Boolean,

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    errorMessage: {
      type: String,
      value: '',
      observer: 'onInvalidOrErrorMessageChanged_',
    },

    /** @private */
    displayErrorMessage_: {
      type: String,
      value: '',
    },

    /**
     * This is strictly used internally for styling, do not attempt to use
     * this to set focus.
     * @private
     */
    focused_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    invalid: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
      observer: 'onInvalidOrErrorMessageChanged_',
    },

    max: {
      type: Number,
      reflectToAttribute: true,
    },

    min: {
      type: Number,
      reflectToAttribute: true,
    },

    maxlength: {
      type: Number,
      reflectToAttribute: true,
    },

    minlength: {
      type: Number,
      reflectToAttribute: true,
    },

    pattern: {
      type: String,
      reflectToAttribute: true,
    },

    inputmode: String,

    label: {
      type: String,
      value: '',
    },

    /** @type {?string} */
    placeholder: {
      type: String,
      value: null,
      observer: 'placeholderChanged_',
    },

    readonly: {
      type: Boolean,
      reflectToAttribute: true,
    },

    required: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {?number} */
    inputTabindex: {
      type: Number,
      value: 0,
      observer: 'onInputTabindexChanged_',
    },

    type: {
      type: String,
      value: 'text',
      observer: 'onTypeChanged_',
    },

    value: {
      type: String,
      value: '',
      notify: true,
      observer: 'onValueChanged_',
    },
  },

  ready() {
    // Use inputTabindex instead.
    assert(!this.hasAttribute('tabindex'));
  },

  /** @private */
  onInputTabindexChanged_() {
    // CrInput only supports 0 or -1 values for the input's tabindex to allow
    // having the input in tab order or not. Values greater than 0 will not work
    // as the shadow root encapsulates tabindices.
    assert(this.inputTabindex === 0 || this.inputTabindex === -1);
  },

  /** @private */
  onTypeChanged_() {
    // Check that the 'type' is one of the supported types.
    assert(SUPPORTED_INPUT_TYPES.has(this.type));
  },

  /** @return {!HTMLInputElement} */
  get inputElement() {
    return /** @type {!HTMLInputElement} */ (this.$.input);
  },

  /**
   * Returns the aria label to be used with the input element.
   * @return {string}
   * @private
   */
  getAriaLabel_(ariaLabel, label, placeholder) {
    return ariaLabel || label || placeholder;
  },

  /**
   * Returns 'true' or 'false' as a string for the aria-invalid attribute.
   * @return {string}
   * @private
   */
  getAriaInvalid_(invalid) {
    return invalid ? 'true' : 'false';
  },

  /** @private */
  onInvalidOrErrorMessageChanged_() {
    this.displayErrorMessage_ = this.invalid ? this.errorMessage : '';

    // On VoiceOver role="alert" is not consistently announced when its content
    // changes. Adding and removing the |role| attribute every time there
    // is an error, triggers VoiceOver to consistently announce.
    const ERROR_ID = 'error';
    const errorElement = this.$$(`#${ERROR_ID}`);
    if (this.invalid) {
      errorElement.setAttribute('role', 'alert');
      this.inputElement.setAttribute('aria-errormessage', ERROR_ID);
    } else {
      errorElement.removeAttribute('role');
      this.inputElement.removeAttribute('aria-errormessage');
    }
  },

  /**
   * This is necessary instead of doing <input placeholder="[[placeholder]]">
   * because if this.placeholder is set to a truthy value then removed, it
   * would show "null" as placeholder.
   * @private
   */
  placeholderChanged_() {
    if (this.placeholder || this.placeholder === '') {
      this.inputElement.setAttribute('placeholder', this.placeholder);
    } else {
      this.inputElement.removeAttribute('placeholder');
    }
  },

  focus() {
    this.focusInput();
  },

  /**
   * Focuses the input element.
   * TODO(crbug.com/882612): Replace this with focus() after resolving the text
   * selection issue described in onFocus_().
   * @return {boolean} Whether the <input> element was focused.
   */
  focusInput() {
    if (this.shadowRoot.activeElement === this.inputElement) {
      return false;
    }
    this.inputElement.focus();
    return true;
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onValueChanged_(newValue, oldValue) {
    if (!newValue && !oldValue) {
      return;
    }
    if (this.autoValidate) {
      this.validate();
    }
  },

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   * @param {!Event} e
   * @private
   */
  onInputChange_(e) {
    this.fire('change', {sourceEvent: e});
  },

  /** @private */
  onInputFocus_() {
    this.focused_ = true;
  },

  /** @private */
  onInputBlur_() {
    this.focused_ = false;
  },

  /**
   * Selects the text within the input. If no parameters are passed, it will
   * select the entire string. Either no params or both params should be passed.
   * Publicly, this function should be used instead of inputElement.select() or
   * manipulating inputElement.selectionStart/selectionEnd because the order of
   * execution between focus() and select() is sensitive.
   * @param {number=} start
   * @param {number=} end
   */
  select(start, end) {
    this.inputElement.focus();
    if (start !== undefined && end !== undefined) {
      this.inputElement.setSelectionRange(start, end);
    } else {
      // Can't just pass one param.
      assert(start === undefined && end === undefined);
      this.inputElement.select();
    }
  },

  /** @return {boolean} */
  validate() {
    this.invalid = !this.inputElement.checkValidity();
    return !this.invalid;
  },
});
