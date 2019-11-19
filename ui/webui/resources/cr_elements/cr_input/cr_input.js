// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

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
 *   tabindex
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

  properties: {
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
      observer: 'disabledChanged_',
    },

    errorMessage: {
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
      reflectToAttribute: true,
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

    label: {
      type: String,
      value: '',
    },

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
    tabindex: {
      type: Number,
      value: 0,
      reflectToAttribute: true,
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

  hostAttributes: {
    'aria-disabled': 'false',
  },

  listeners: {
    'focus': 'onFocus_',
    'pointerdown': 'onPointerDown_',
  },

  /** @private {?number} */
  originalTabIndex_: null,

  /** @override */
  attached: function() {
    // Run this for the first time in attached instead of in disabledChanged_
    // since this.tabindex might not be set yet then.
    if (this.disabled) {
      this.reconcileTabindex_();
    }
  },

  /** @private */
  onTypeChanged_: function() {
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
  getAriaLabel_: function(ariaLabel, label, placeholder) {
    return ariaLabel || label || placeholder;
  },

  /** @private */
  disabledChanged_: function(current, previous) {
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    // In case input was focused when disabled changes.
    this.focused_ = false;

    // Don't change tabindex until after finished attaching, since this.tabindex
    // might not be intialized yet.
    if (previous !== undefined) {
      this.reconcileTabindex_();
    }
  },

  /**
   * This helper function manipulates the tabindex based on disabled state. If
   * this element is disabled, this function will remember the tabindex and
   * unset it. If the element is enabled again, it will restore the tabindex
   * to it's previous value.
   * @private
   */
  reconcileTabindex_: function() {
    if (this.disabled) {
      this.recordAndUnsetTabIndex_();
    } else {
      this.restoreTabIndex_();
    }
  },

  /**
   * This is necessary instead of doing <input placeholder="[[placeholder]]">
   * because if this.placeholder is set to a truthy value then removed, it
   * would show "null" as placeholder.
   * @private
   */
  placeholderChanged_: function() {
    if (this.placeholder || this.placeholder == '') {
      this.inputElement.setAttribute('placeholder', this.placeholder);
    } else {
      this.inputElement.removeAttribute('placeholder');
    }
  },

  /** @private */
  onFocus_: function() {
    if (!this.focusInput()) {
      return;
    }
    // Always select the <input> element on focus. TODO(stevenjb/scottchen):
    // Native <input> elements only do this for keyboard focus, not when
    // focus() is called directly. Fix this? https://crbug.com/882612.
    this.inputElement.select();
  },

  /**
   * Focuses the input element.
   * TODO(crbug.com/882612): Replace this with focus() after resolving the text
   * selection issue described in onFocus_().
   * @return {boolean} Whether the <input> element was focused.
   */
  focusInput: function() {
    if (this.shadowRoot.activeElement == this.inputElement) {
      return false;
    }
    this.inputElement.focus();
    return true;
  },

  /** @private */
  recordAndUnsetTabIndex_: function() {
    // Don't change originalTabIndex_ if it just got changed.
    if (this.originalTabIndex_ === null) {
      this.originalTabIndex_ = this.tabindex;
    }

    this.tabindex = null;
  },

  /** @private */
  restoreTabIndex_: function() {
    this.tabindex = this.originalTabIndex_;
    this.originalTabIndex_ = null;
  },

  /**
   * Prevents clicking random spaces within cr-input but outside of <input>
   * from triggering focus.
   * @param {!Event} e
   * @private
   */
  onPointerDown_: function(e) {
    // Don't need to manipulate tabindex if cr-input is already disabled.
    if (this.disabled) {
      return;
    }

    // Should not mess with tabindex when <input> is clicked, otherwise <input>
    // will lose and regain focus, and replay the focus animation.
    if (e.path[0].tagName !== 'INPUT') {
      this.recordAndUnsetTabIndex_();
      setTimeout(() => {
        // Restore tabindex, unless disabled in the same cycle as pointerdown.
        if (!this.disabled) {
          this.restoreTabIndex_();
        }
      }, 0);
    }
  },

  /**
   * When shift-tab is pressed, first bring the focus to the host element.
   * This accomplishes 2 things:
   * 1) Host doesn't get focused when the browser moves the focus backward.
   * 2) focus now escaped the shadow-dom of this element, so that it'll
   *    correctly obey non-zero tabindex ordering of the containing document.
   * @private
   */
  onInputKeydown_: function(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onValueChanged_: function(newValue, oldValue) {
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
  onInputChange_: function(e) {
    this.fire('change', {sourceEvent: e});
  },

  /** @private */
  onInputFocus_: function() {
    this.focused_ = true;
  },

  /** @private */
  onInputBlur_: function() {
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
  select: function(start, end) {
    this.focusInput();
    if (start !== undefined && end !== undefined) {
      this.inputElement.setSelectionRange(start, end);
    } else {
      // Can't just pass one param.
      assert(start === undefined && end === undefined);
      this.inputElement.select();
    }
  },

  /** @return {boolean} */
  validate: function() {
    this.invalid = !this.inputElement.checkValidity();
    return !this.invalid;
  },
});
})();
