// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A toolbar for the ImageEditor.
 */
class ImageEditorToolbar extends cr.EventTarget {
  /**
   * @param {!HTMLElement} parent The parent element.
   * @param {function(string)} displayStringFunction A string formatting
   *     function.
   * @param {function(Object)=} opt_updateCallback The callback called when
   *     controls change.
   * @param {boolean=} opt_showActionButtons True to show action buttons.
   */
  constructor(
      parent, displayStringFunction, opt_updateCallback,
      opt_showActionButtons) {
    super();

    this.wrapper_ = parent;
    this.displayStringFunction_ = displayStringFunction;

    /**
     * @type {?function(Object)}
     * @private
     */
    this.updateCallback_ = opt_updateCallback || null;

    /**
     * @private {!HTMLElement}
     */
    this.container_ =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    this.container_.classList.add('container');
    this.wrapper_.appendChild(this.container_);

    // Create action buttons.
    if (opt_showActionButtons) {
      var actionButtonsLayer = document.createElement('div');
      actionButtonsLayer.classList.add('action-buttons');

      this.cancelButton_ = ImageEditorToolbar.createButton_(
          'GALLERY_CANCEL_LABEL',
          ImageEditorToolbar.ButtonType.LABEL_UPPER_CASE,
          this.onCancelClicked_.bind(this), 'cancel');
      actionButtonsLayer.appendChild(this.cancelButton_);

      this.doneButton_ = ImageEditorToolbar.createButton_(
          'GALLERY_DONE', ImageEditorToolbar.ButtonType.LABEL_UPPER_CASE,
          this.onDoneClicked_.bind(this), 'done');
      actionButtonsLayer.appendChild(this.doneButton_);

      this.wrapper_.appendChild(actionButtonsLayer);
    }
  }

  /**
   * Handles click event of done button.
   * @private
   */
  onDoneClicked_() {
    (/** @type {!PaperRipple}*/ (
         this.doneButton_.querySelector('paper-ripple')))
        .simulatedRipple();

    var event = new Event('done-clicked');
    this.dispatchEvent(event);
  }

  /**
   * Handles click event of cancel button.
   * @private
   */
  onCancelClicked_() {
    (/**@type{PaperRipple}*/ (this.cancelButton_.querySelector('paper-ripple')))
        .simulatedRipple();

    var event = new Event('cancel-clicked');
    this.dispatchEvent(event);
  }

  /**
   * Returns the parent element.
   * @return {!HTMLElement}
   */
  getElement() {
    return this.container_;
  }

  /**
   * Clear the toolbar.
   */
  clear() {
    ImageUtil.removeChildren(this.container_);
  }

  /**
   * Add a control.
   * @param {!HTMLElement} element The control to add.
   * @return {!HTMLElement} The added element.
   */
  add(element) {
    this.container_.appendChild(element);
    return element;
  }

  /**
   * Create a button.
   *
   * @param {string} title String ID of button title.
   * @param {ImageEditorToolbar.ButtonType} type Button type.
   * @param {function(Event)} handler onClick handler.
   * @param {string=} opt_class Extra class name.
   * @return {!HTMLElement} The created button.
   * @private
   */
  static createButton_(title, type, handler, opt_class) {
    var button = /** @type {!HTMLElement} */ (document.createElement('button'));
    if (opt_class) {
      button.classList.add(opt_class);
    }
    button.classList.add('edit-toolbar');

    if (type === ImageEditorToolbar.ButtonType.ICON ||
        type === ImageEditorToolbar.ButtonType.ICON_TOGGLEABLE) {
      var icon = document.createElement('div');
      icon.classList.add('icon');

      // Show tooltip for icon button.
      assertInstanceof(document.querySelector('files-tooltip'), FilesTooltip)
          .addTarget(button);

      button.appendChild(icon);

      if (type === ImageEditorToolbar.ButtonType.ICON) {
        var filesRipple = document.createElement('files-ripple');
        button.appendChild(filesRipple);
      } else {
        var filesToggleRipple = document.createElement('files-toggle-ripple');
        button.appendChild(filesToggleRipple);
      }
    } else if (
        type === ImageEditorToolbar.ButtonType.LABEL ||
        type === ImageEditorToolbar.ButtonType.LABEL_UPPER_CASE) {
      var label = document.createElement('span');
      label.classList.add('label');
      label.textContent =
          type === ImageEditorToolbar.ButtonType.LABEL_UPPER_CASE ?
          strf(title).toLocaleUpperCase() :
          strf(title);

      button.appendChild(label);

      var paperRipple = document.createElement('paper-ripple');
      button.appendChild(paperRipple);
    } else {
      assertNotReached();
    }

    button.label = strf(title);
    button.setAttribute('aria-label', strf(title));

    GalleryUtil.decorateMouseFocusHandling(button);

    button.addEventListener('click', handler, false);
    button.addEventListener('keydown', function(event) {
      // Stop propagation of Enter key event to prevent it from being captured
      // by image editor.
      if (event.key === 'Enter') {
        event.stopPropagation();
      }
    });

    return button;
  }

  /**
   * Add a button.
   *
   * @param {string} title Button title.
   * @param {ImageEditorToolbar.ButtonType} type Button type.
   * @param {function(Event)} handler onClick handler.
   * @param {string=} opt_class Extra class name.
   * @return {!HTMLElement} The added button.
   */
  addButton(title, type, handler, opt_class) {
    var button =
        ImageEditorToolbar.createButton_(title, type, handler, opt_class);
    this.add(button);
    return button;
  }

  /**
   * Add a input field.
   *
   * @param {string} name Input name
   * @param {string} title Input title
   * @param {function(Event)} handler onInput and onChange handler
   * @param {string|number} value Default value
   * @param {string=} opt_unit Unit for an input field
   * @return {!HTMLElement} Input Element
   */
  addInput(name, title, handler, value, opt_unit) {
    var input = /** @type {!HTMLElement} */ (document.createElement('div'));
    input.classList.add('input', name);

    var text = document.createElement('cr-input');
    text.setAttribute('label', strf(title));
    text.classList.add('text', name);
    text.value = value;

    // We should listen to not only 'change' event, but also 'input' because we
    // want to update values as soon as the user types characters.
    text.addEventListener('input', handler, false);
    text.addEventListener('change', handler, false);
    input.appendChild(text);

    if (opt_unit) {
      var unitLabel = document.createElement('span');
      unitLabel.textContent = opt_unit;
      unitLabel.classList.add('unit_label');
      input.appendChild(unitLabel);
    }

    input.name = name;
    input.getValue = function(text) {
      return text.value;
    }.bind(this, text);
    input.setValue = function(text, value) {
      text.value = value;
    }.bind(this, text);

    this.add(input);

    return input;
  }

  /**
   * Add a range control (scalar value picker).
   *
   * @param {string} name An option name.
   * @param {string} title An option title.
   * @param {number} min Min value of the option.
   * @param {number} value Default value of the option.
   * @param {number} max Max value of the options.
   * @param {number=} opt_scale A number to multiply by when setting
   *     min/value/max in DOM.
   * @param {boolean=} opt_showNumeric True if numeric value should be
   *     displayed.
   * @return {!HTMLElement} Range element.
   */
  addRange(name, title, min, value, max, opt_scale, opt_showNumeric) {
    var range = /** @type {!HTMLElement} */ (document.createElement('div'));
    range.classList.add('range', name);

    var icon = document.createElement('icon');
    icon.classList.add('icon');
    range.appendChild(icon);

    var label = document.createElement('span');
    label.textContent = strf(title);
    label.classList.add('label');
    range.appendChild(label);

    var scale = opt_scale || 1;
    var slider = document.createElement('cr-slider');
    slider.min = Math.ceil(min * scale);
    slider.max = Math.floor(max * scale);
    slider.value = value * scale;
    const handler = () => {
      if (this.updateCallback_) {
        this.updateCallback_(this.getOptions());
      }
    };
    slider.addEventListener('cr-slider-value-changed', handler);
    setTimeout(handler);
    range.appendChild(slider);

    range.name = name;
    range.getValue = () => slider.value / scale;

    // Swallow the left and right keys, so they are not handled by other
    // listeners.
    range.addEventListener('keydown', function(e) {
      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        e.stopPropagation();
      }
    });

    this.add(range);

    return range;
  }

  /**
   * @return {!Object} options A map of options.
   */
  getOptions() {
    var values = {};

    for (var child = this.container_.firstChild; child;
         child = child.nextSibling) {
      if (child.name) {
        values[child.name] = child.getValue();
      }
    }

    return values;
  }

  /**
   * Reset the toolbar.
   */
  reset() {
    for (var child = this.wrapper_.firstChild; child;
         child = child.nextSibling) {
      if (child.reset) {
        child.reset();
      }
    }
  }

  /**
   * Show/hide the toolbar.
   * @param {boolean} on True if show.
   */
  show(on) {
    if (!this.wrapper_.firstChild) {
      return;  // Do not show empty toolbar;
    }

    this.wrapper_.hidden = !on;

    // Focus the first input on the toolbar.
    if (on) {
      var input = this.container_.querySelector(
          // Crop aspect ratio buttons should not be focused immediately
          // crbug.com/655943
          [
            'button:not(.crop-aspect-ratio)',
            'cr-button',
            'input',
            'cr-slider',
            'cr-input',
          ].join(', '));
      if (input) {
        input.focus();
        // Fix for crbug/914741 set selection to the end (> 32-bit int)
        // Note the input element lives in Shadow DOM.
        if (input.select && input.tagName) {
          assert(input.tagName === 'CR-INPUT');
          input.select(12, 12);
        }
      }
    }
  }
}

/**
 * Height of the toolbar.
 * @const {number}
 */
ImageEditorToolbar.HEIGHT = 48;  // px

/**
 * Button type.
 * @enum {string}
 */
ImageEditorToolbar.ButtonType = {
  ICON: 'icon',
  ICON_TOGGLEABLE: 'icon_toggleable',
  LABEL: 'label',
  LABEL_UPPER_CASE: 'label_upper_case'
};
