// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a common button control, bound to command.
 */

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import {Command} from './command.js';

/**
 * Creates a new button element.
 */
export class CommandButton extends CrButtonElement {
  /**
   * Associated command.
   */
  private command_: null|Command = null;

  initialize() {
    const commandId = this.getAttribute('command');
    if (commandId) {
      this.setCommand(commandId);
    }

    this.addEventListener('click', this.handleClick_.bind(this));
  }

  /**
   * Returns associated command.
   */
  getCommand(): null|Command {
    return this.command_;
  }

  /**
   * Associates command with this button.
   * @param command Command id, or command object to associate with this button.
   */
  setCommand(command: string|Command) {
    if (this.command_) {
      this.command_.removeEventListener('labelChange', this);
      this.command_.removeEventListener('disabledChange', this);
      this.command_.removeEventListener('hiddenChange', this);
    }

    if (typeof command === 'string') {
      assert(command[0] === '#');
      command = this.ownerDocument.body.querySelector<Command>(command)!;
      assert(command);
      crInjectTypeAndInit(command, Command);
    }

    this.command_ = command;
    if (command) {
      if (command.id) {
        this.setAttribute('command', '#' + command.id);
      }

      this.setLabel(command.label);
      this.disabled = command.disabled;
      this.hidden = command.hidden;

      this.command_.addEventListener('labelChange', this);
      this.command_.addEventListener('disabledChange', this);
      this.command_.addEventListener('hiddenChange', this);
    }
  }

  /**
   * Returns button label
   */
  getLabel(): string {
    return this.command_ ? this.command_.label : '';
  }

  /**
   * Sets button label.
   */
  setLabel(label: string) {
    // Swap the textContent with current label only when this button doesn't
    // have any elements as children.
    //
    // TODO(fukino): If a user customize the button content, it becomes the
    // user's responsibility to update the content on command label's change.
    // Updating the label in customized button content should be done
    // automatically by specifying an element which should be synced with the
    // command label using class name or polymer's template binding.
    if (!this.firstElementChild) {
      this.textContent = label;
    }
  }

  /**
   * Handles click event and dispatches associated command.
   * @param _e The mouseup event object.
   */
  private handleClick_(_e: Event) {
    if (!this.disabled && this.command_) {
      this.command_.execute(this);
    }
  }

  /**
   * Handles changes to the associated command.
   */
  handleEvent(e: Event) {
    switch (e.type) {
      case 'disabledChange':
        assert(this.command_);
        this.disabled = this.command_.disabled;
        break;
      case 'hiddenChange':
        assert(this.command_);
        this.hidden = this.command_.hidden;
        break;
      case 'labelChange':
        assert(this.command_);
        this.setLabel(this.command_.label);
        break;
    }
  }
}
