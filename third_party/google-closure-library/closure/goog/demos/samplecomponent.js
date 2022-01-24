/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A simple, sample component.
 */
goog.provide('goog.demos.SampleComponent');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.events.KeyCodes');
goog.require('goog.events.KeyHandler');
goog.require('goog.ui.Component');
goog.requireType('goog.events.Event');



/**
 * A simple box that changes colour when clicked. This class demonstrates the
 * goog.ui.Component API, and is keyboard accessible, as per
 * http://wiki/Main/ClosureKeyboardAccessible
 * @final
 * @unrestricted
 */
goog.demos.SampleComponent = class extends goog.ui.Component {
  /**
   * @param {string=} opt_label A label to display. Defaults to "Click Me" if
   *     none provided.
   * @param {goog.dom.DomHelper=} opt_domHelper DOM helper to use.
   */
  constructor(opt_label, opt_domHelper) {
    super(opt_domHelper);

    /**
     * The label to display.
     * @type {string}
     * @private
     */
    this.initialLabel_ = opt_label || 'Click Me';

    /**
     * The current color.
     * @type {string}
     * @private
     */
    this.color_ = 'red';

    /**
     * Keyboard handler for this object. This object is created once the
     * component's DOM element is known.
     *
     * @type {goog.events.KeyHandler?}
     * @private
     */
    this.kh_ = null;
  }

  /**
   * Changes the color of the element.
   * @private
   */
  changeColor_() {
    if (this.color_ == 'red') {
      this.color_ = 'green';
    } else if (this.color_ == 'green') {
      this.color_ = 'blue';
    } else {
      this.color_ = 'red';
    }
    this.getElement().style.backgroundColor = this.color_;
  }

  /**
   * Creates an initial DOM representation for the component.
   * @override
   */
  createDom() {
    this.decorateInternal(this.dom_.createElement(goog.dom.TagName.DIV));
  }

  /**
   * Decorates an existing HTML DIV element as a SampleComponent.
   *
   * @param {Element} element The DIV element to decorate. The element's
   *    text, if any will be used as the component's label.
   * @override
   */
  decorateInternal(element) {
    super.decorateInternal(element);
    if (!this.getLabelText()) {
      this.setLabelText(this.initialLabel_);
    }

    const elem = this.getElement();
    goog.dom.classlist.add(elem, goog.getCssName('goog-sample-component'));
    elem.style.backgroundColor = this.color_;
    elem.tabIndex = 0;

    this.kh_ = new goog.events.KeyHandler(elem);
    this.getHandler().listen(
        this.kh_, goog.events.KeyHandler.EventType.KEY, this.onKey_);
  }

  /** @override */
  disposeInternal() {
    super.disposeInternal();
    if (this.kh_) {
      this.kh_.dispose();
    }
  }

  /**
   * Called when component's element is known to be in the document.
   * @override
   */
  enterDocument() {
    super.enterDocument();
    this.getHandler().listen(
        this.getElement(), goog.events.EventType.CLICK, this.onDivClicked_);
  }

  /**
   * Gets the current label text.
   *
   * @return {string} The current text set into the label, or empty string if
   *     none set.
   */
  getLabelText() {
    if (!this.getElement()) {
      return '';
    }
    return goog.dom.getTextContent(this.getElement());
  }

  /**
   * Handles DIV element clicks, causing the DIV's colour to change.
   * @param {goog.events.Event} event The click event.
   * @private
   */
  onDivClicked_(event) {
    this.changeColor_();
  }

  /**
   * Fired when user presses a key while the DIV has focus. If the user presses
   * space or enter, the color will be changed.
   * @param {goog.events.Event} event The key event.
   * @private
   */
  onKey_(event) {
    const keyCodes = goog.events.KeyCodes;
    if (event.keyCode == keyCodes.SPACE || event.keyCode == keyCodes.ENTER) {
      this.changeColor_();
    }
  }

  /**
   * Sets the current label text. Has no effect if component is not rendered.
   *
   * @param {string} text The text to set as the label.
   */
  setLabelText(text) {
    if (this.getElement()) {
      goog.dom.setTextContent(this.getElement(), text);
    }
  }
};
