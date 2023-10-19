// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a common button control, bound to command.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';
// @ts-ignore: error TS6192: All imports in import declaration are unused.
import {getPropertyDescriptor, PropertyKind} from 'chrome://resources/ash/common/cr_deprecated.js';
import {decorate} from '../../../common/js/ui.js';
import {Command} from './command.js';

/**
 * Creates a new button element.
 * @extends {HTMLButtonElement}
 */
export class CommandButton {
  constructor() {
    /**
     * Associated command.
     * @private @type {Command}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'Command'.
    this.command_ = null;
  }

  /**
   * Decorates the given element.
   * @param {!Element} element Item to be decorated.
   * @return {!CommandButton} Decorated item.
   */
  static decorate(element) {
    // Add the CommandButton methods to the element we're
    // decorating, leaving it's prototype chain intact.
    Object.getOwnPropertyNames(CommandButton.prototype).forEach(name => {
      if (name !== 'constructor') {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'CommandButton'.
        element[name] = CommandButton.prototype[name];
      }
    });
    // @ts-ignore: error TS2352: Conversion of type 'Element' to type
    // 'CommandButton' may be a mistake because neither type sufficiently
    // overlaps with the other. If this was intentional, convert the expression
    // to 'unknown' first.
    element = /** @type {!CommandButton} */ (element);
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'Element'.
    element.decorate();
    // @ts-ignore: error TS2740: Type 'Element' is missing the following
    // properties from type 'CommandButton': command_, decorate, getCommand,
    // setCommand, and 7 more.
    return element;
  }

  /**
   * Initializes the menu item.
   */
  decorate() {
    let commandId;
    // @ts-ignore: error TS2339: Property 'getAttribute' does not exist on type
    // 'CommandButton'.
    if ((commandId = this.getAttribute('command'))) {
      this.setCommand(commandId);
    }

    // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
    // type 'CommandButton'.
    this.addEventListener('click', this.handleClick_.bind(this));
  }

  /**
   * Returns associated command.
   * @return {Command} associated command.
   */
  getCommand() {
    // @ts-ignore: error TS2322: Type 'Command | null' is not assignable to type
    // 'Command'.
    return this.command_;
  }

  /**
   * Associates command with this button.
   * @param {string|Command} command Command id, or command object to
   * associate with this button.
   */
  setCommand(command) {
    if (this.command_) {
      this.command_.removeEventListener(
          'labelChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
      this.command_.removeEventListener(
          'disabledChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
      this.command_.removeEventListener(
          'hiddenChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
    }

    if (typeof command == 'string') {
      assert(command[0] == '#');
      command = /** @type {!Command} */
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type 'CommandButton'.
          (this.ownerDocument.body.querySelector(command));
      decorate(command, Command);
    }

    this.command_ = command;
    if (command) {
      if (command.id) {
        // @ts-ignore: error TS2339: Property 'setAttribute' does not exist on
        // type 'CommandButton'.
        this.setAttribute('command', '#' + command.id);
      }

      this.setLabel(command.label);
      this.disabled = command.disabled;
      this.hidden = command.hidden;

      this.command_.addEventListener(
          'labelChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
      this.command_.addEventListener(
          'disabledChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
      this.command_.addEventListener(
          'hiddenChange',
          // @ts-ignore: error TS2352: Conversion of type 'this' to type
          // 'EventListener' may be a mistake because neither type sufficiently
          // overlaps with the other. If this was intentional, convert the
          // expression to 'unknown' first.
          /** @type {EventListener} */ (this));
    }
  }

  /**
   * Returns button label
   * @return {string} Button label.
   */
  getLabel() {
    return this.command_ ? this.command_.label : '';
  }

  /**
   * Sets button label.
   * @param {string} label New button label.
   */
  setLabel(label) {
    // Swap the textContent with current label only when this button doesn't
    // have any elements as children.
    //
    // TODO(fukino): If a user customize the button content, it becomes the
    // user's responsibility to update the content on command label's change.
    // Updating the label in customized button content should be done
    // automatically by specifying an element which should be synced with the
    // command label using class name or polymer's template binding.
    // @ts-ignore: error TS2339: Property 'firstElementChild' does not exist on
    // type 'CommandButton'.
    if (!this.firstElementChild) {
      this.textContent = label;
    }
  }

  /**
   * Handles click event and dispatches associated command.
   * @param {Event} e The mouseup event object.
   * @private
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleClick_(e) {
    if (!this.disabled && this.command_) {
      // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
      // parameter of type 'HTMLElement | undefined'.
      this.command_.execute(this);
    }
  }

  /**
   * Handles changes to the associated command.
   * @param {Event} e The event object.
   */
  handleEvent(e) {
    switch (e.type) {
      case 'disabledChange':
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.disabled = this.command_.disabled;
        break;
      case 'hiddenChange':
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.hidden = this.command_.hidden;
        break;
      case 'labelChange':
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.setLabel(this.command_.label);
        break;
    }
  }
}

/**
 * Whether the button is disabled or not.
 * @type {boolean}
 */
CommandButton.prototype.disabled;

/**
 * Whether the button is hidden or not.
 * @type {boolean}
 */
CommandButton.prototype.hidden;

CommandButton.prototype.__proto__ = HTMLButtonElement.prototype;
